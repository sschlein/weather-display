// Weather from two free services that need no account: Open-Meteo for the
// temperature, the week and rain beyond two hours, and the DWD's rain radar
// through Bright Sky for the next two hours, in steps of five minutes. The
// radar sees rain that is already falling and where it moves; it is what
// "in 25 Minuten" is worth trusting.

#include "weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp32/rom/miniz.h>
#include <mbedtls/base64.h>
#include <math.h>

#include "config.h"
#include "ui.h"

Current current;
Day days[7];
int dayCount = 0;
Series radar, model;
time_t forecastAt = 0, radarAt = 0;

float Series::at(time_t t) const {
  if (count == 0 || t < first - step) return NAN;
  int i = (t - (first - step)) / step;
  return i < count ? mmh[i] : NAN;
}

namespace {

/** The response body, or "" on failure. Neither service needs a secret, so
 * the certificate isn't checked. */
String get(const String &url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true);  // no chunked encoding, so the body can be read as is
  if (!http.begin(client, url)) return "";
  int status = http.GET();
  String body = status == 200 ? http.getString() : "";
  if (status != 200) Serial.printf("GET %s: %d\n", url.c_str(), status);
  http.end();
  return body;
}

String isoUtc(time_t t) {
  struct tm utc;
  gmtime_r(&t, &utc);
  char text[24];
  strftime(text, sizeof text, "%Y-%m-%dT%H:%MZ", &utc);
  return text;
}

/** One radar pixel out of Bright Sky's "compressed" format: base64 of
 * zlib-compressed little-endian uint16, row by row, in 0.01 mm per 5 min. */
int radarPixel(const char *base64, int index) {
  static uint8_t packed[512], raw[512];
  size_t packedLength = 0;
  if (mbedtls_base64_decode(packed, sizeof packed, &packedLength, (const uint8_t *)base64, strlen(base64)) != 0) {
    return -1;
  }
  // Keeps its 11 KB of state on the stack: main.cpp gives the loop room.
  size_t rawLength = tinfl_decompress_mem_to_mem(raw, sizeof raw, packed, packedLength, TINFL_FLAG_PARSE_ZLIB_HEADER);
  if (rawLength == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED || (size_t)(index * 2 + 1) >= rawLength) return -1;
  return raw[index * 2] | (raw[index * 2 + 1] << 8);
}

}  // namespace

bool refreshForecast() {
  String body = get(String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LATITUDE, 4) +
                    "&longitude=" + String(LONGITUDE, 4) +
                    "&current=temperature_2m,apparent_temperature,weather_code,is_day"
                    "&minutely_15=precipitation&past_minutely_15=1&forecast_minutely_15=48"
                    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,"
                    "precipitation_probability_max&forecast_days=7"
                    "&timezone=Europe%2FBerlin&timeformat=unixtime");
  if (body.isEmpty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;

  JsonObject now = doc["current"];
  current.ok = !now["temperature_2m"].isNull();
  current.temperature = now["temperature_2m"] | NAN;
  current.feelsLike = now["apparent_temperature"] | NAN;
  current.code = now["weather_code"] | -1;
  current.day = (now["is_day"] | 1) == 1;

  JsonArray times = doc["minutely_15"]["time"], amounts = doc["minutely_15"]["precipitation"];
  model.count = 0;
  model.step = 900;
  model.first = times[0] | 0;
  for (size_t i = 0; i < amounts.size() && model.count < Series::CAPACITY; i++) {
    model.mmh[model.count++] = (amounts[i] | 0.0f) * 4;
  }

  JsonObject daily = doc["daily"];
  int n = min((int)daily["time"].size(), 7);
  for (int i = 0; i < n; i++) {
    days[i] = {daily["time"][i] | 0, daily["weather_code"][i] | -1, daily["temperature_2m_min"][i] | NAN,
               daily["temperature_2m_max"][i] | NAN, daily["precipitation_sum"][i] | NAN,
               daily["precipitation_probability_max"][i] | -1};
  }
  dayCount = n;
  forecastAt = time(nullptr);
  Serial.printf("forecast: %.1f °C, code %d, %d days, %d rain steps\n", current.temperature, current.code, dayCount,
                model.count);
  return current.ok;
}

bool refreshRadar() {
  if (!clockSet()) return false;
  time_t now = time(nullptr);
  String body = get(String("https://api.brightsky.dev/radar?lat=") + String(LATITUDE, 4) +
                    "&lon=" + String(LONGITUDE, 4) + "&distance=1000&date=" + isoUtc(now - 600) +
                    "&last_date=" + isoUtc(now + 3 * 3600));
  if (body.isEmpty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;

  // A few pixels around the place; ours is where latlon_position points.
  // bbox is [top, left, bottom, right] in radar pixels.
  JsonArray bbox = doc["bbox"];
  int width = (bbox[3] | 0) - (bbox[1] | 0) + 1;
  int px = (int)floorf(doc["latlon_position"]["x"] | 0.0f), py = (int)floorf(doc["latlon_position"]["y"] | 0.0f);
  int index = py * width + px;

  Series next;
  next.step = 300;
  for (JsonObject frame : doc["radar"].as<JsonArray>()) {
    time_t at = parseIso(frame["timestamp"] | "");
    int value = radarPixel(frame["precipitation_5"] | "", index);
    if (!at || value < 0 || next.count >= Series::CAPACITY) continue;
    if (next.count == 0) next.first = at;
    // A frame missing in between: keep the steps even.
    while (next.count < Series::CAPACITY && next.first + next.count * next.step < at) {
      next.mmh[next.count] = next.count ? next.mmh[next.count - 1] : 0;
      next.count++;
    }
    if (next.count < Series::CAPACITY) next.mmh[next.count++] = value * 0.01f * 12;
  }
  if (next.count == 0) return false;
  radar = next;
  radarAt = now;
  float peak = 0;
  for (int i = 0; i < radar.count; i++) peak = max(peak, radar.mmh[i]);
  Serial.printf("radar: %d frames until %s, peak %.2f mm/h\n", radar.count, clockText(radar.end()).c_str(), peak);
  return true;
}

bool fromRadar(time_t t) {
  // Radar older than half an hour has nothing to say about now.
  return radar.count && time(nullptr) - radarAt < 1800 && !isnan(radar.at(t));
}

float rainAt(time_t t) { return fromRadar(t) ? radar.at(t) : model.at(t); }

// MARK: - Outlook

namespace {

bool snowing() { return current.ok && current.temperature <= 1.0f; }

/** The first time from `from` on, in 5-minute steps up to `until`, when it
 * is wet (or, with `wet` false, dry for at least a quarter hour). 0 if never. */
time_t firstWhen(bool wet, time_t from, time_t until) {
  for (time_t t = from; t <= until; t += 300) {
    float v = rainAt(t);
    if (isnan(v)) return 0;
    if (wet && v >= WET_MMH) return t;
    if (!wet && v < WET_MMH) {
      bool stays = true;
      for (time_t u = t + 300; u < t + 900 && u <= until; u += 300) stays &= !(rainAt(u) >= WET_MMH);
      if (stays) return t;
    }
  }
  return 0;
}

float peakBetween(time_t from, time_t until) {
  float peak = 0;
  for (time_t t = from; t < until; t += 300) {
    float v = rainAt(t);
    if (!isnan(v)) peak = max(peak, v);
  }
  return peak;
}

/** The horizon: how far ahead either source reaches. */
time_t reach() {
  time_t end = 0;
  if (radar.count && time(nullptr) - radarAt < 1800) end = radar.end();
  if (model.count) end = max(end, model.end());
  return end;
}

/** Minutes until `t`, to the nearest five. */
int minutesUntil(time_t t) {
  long seconds = t - time(nullptr);
  return max(5L, (seconds + 150) / 300 * 5);
}

/** "17:15", to the quarter hour. */
String roughly(time_t t) { return clockText((t + 450) / 900 * 900); }

String duration(int minutes) {
  if (minutes < 60) return String(minutes) + " Minuten";
  if (minutes < 90) return "eine Stunde";
  return decimal(minutes / 60.0f, minutes % 60 ? 1 : 0) + " Stunden";
}

}  // namespace

String intensityText(float mmh) {
  // The DWD's limits: light below 2.5 mm/h, heavy from 10 mm/h.
  const char *what = snowing() ? "Schnee" : "Regen";
  const char *how = mmh < 2.5f ? "Leichter " : (mmh < 10 ? "Mäßiger " : "Starker ");
  return String(how) + what;
}

RainOutlook rainOutlook() {
  RainOutlook out = {"", "", false, false};
  time_t now = time(nullptr), until = reach();
  float here = rainAt(now);
  const char *verb = snowing() ? "schneien" : "regnen";
  if (isnan(here) || until <= now) {
    out.headline = "Keine Regendaten";
    out.detail = "Radar und Vorhersage sind nicht erreichbar";
    return out;
  }
  // Minutes are worth saying as long as the radar reaches; beyond, a time.
  time_t radarEnd = fromRadar(now) ? radar.end() : now + 3600;

  if (here >= WET_MMH) {
    out.wet = true;
    time_t stops = firstWhen(false, now + 300, until);
    String kind = intensityText(peakBetween(now, stops ? stops : now + 3600));
    if (!stops) {
      out.headline = String("Es ") + (snowing() ? "schneit" : "regnet");
      out.detail = kind + ", noch mindestens " + duration(max(30L, (until - now) / 60 / 30 * 30));
    } else if (stops <= radarEnd) {
      int minutes = minutesUntil(stops);
      out.headline = minutes <= 5 ? String("Es hört gleich auf zu ") + verb
                                  : "Es hört in " + String(minutes) + " Minuten auf zu " + verb;
      time_t again = firstWhen(true, stops, until);
      out.detail = kind + (again ? ", ab etwa " + roughly(again) + " wieder" : ", danach trocken");
    } else {
      out.headline = String("Es ") + (snowing() ? "schneit" : "regnet") + " bis etwa " + roughly(stops);
      out.detail = kind;
    }
    return out;
  }

  time_t starts = firstWhen(true, now + 300, until);
  if (!starts) {
    out.headline = snowing() ? "Kein Schnee in Sicht" : "Kein Regen in Sicht";
    long hours = (until - now) / 3600;
    out.detail = hours >= 2 ? "Die nächsten " + String(hours) + " Stunden bleibt es trocken"
                            : String("Die nächste Stunde bleibt es trocken");
    return out;
  }
  time_t stops = firstWhen(false, starts, until);
  String kind = intensityText(peakBetween(starts, stops ? stops : until));
  if (starts <= radarEnd) {
    out.soon = true;
    int minutes = minutesUntil(starts);
    out.headline = minutes <= 5 ? String("Es beginnt gleich zu ") + verb
                                : "Es beginnt in " + String(minutes) + " Minuten zu " + verb;
    out.detail = stops ? kind + ", etwa " + duration(max(5L, (stops - starts) / 60)) + " lang" : kind + ", länger anhaltend";
  } else {
    out.headline = "Trocken bis etwa " + roughly(starts);
    String lower = kind;
    lower.setCharAt(0, tolower(lower[0]));
    out.detail = "Danach " + lower;
  }
  return out;
}
