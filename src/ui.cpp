#include "ui.h"

#include <math.h>
#include <time.h>

bool clockSet() { return time(nullptr) > 1700000000; }

String clockText(time_t at) {
  struct tm local;
  localtime_r(&at, &local);
  char text[8];
  strftime(text, sizeof text, "%H:%M", &local);
  return text;
}

String clockText() { return clockSet() ? clockText(time(nullptr)) : ""; }

String dateText() {
  static const char *DAYS[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
  static const char *MONTHS[] = {"Januar", "Februar", "März", "April", "Mai", "Juni",
                                 "Juli", "August", "September", "Oktober", "November", "Dezember"};
  if (!clockSet()) return "";
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  return String(DAYS[local.tm_wday]) + ", " + local.tm_mday + ". " + MONTHS[local.tm_mon];
}

// Howard Hinnant's days_from_civil.
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

time_t parseIso(const String &value) {
  int y, mo, d, h, mi, s;
  if (sscanf(value.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return 0;
  time_t t = (time_t)daysFromCivil(y, mo, d) * DAY + h * 3600 + mi * 60 + s;
  // Offset: the last "+hh:mm" or "-hh:mm" after the time.
  int plus = value.lastIndexOf('+'), minus = value.lastIndexOf('-');
  int at = plus > 10 ? plus : (minus > 10 ? minus : -1);
  if (at > 0) {
    int sign = at == plus ? 1 : -1;
    int oh = value.substring(at + 1, at + 3).toInt(), om = value.substring(at + 4, at + 6).toInt();
    t -= sign * (oh * 3600 + om * 60);
  }
  return t;
}

String decimal(float value, int decimals) {
  String text = String(value, decimals);
  text.replace('.', ',');
  if (text == "-0" || text == "-0,0") text.remove(0, 1);
  return text;
}

String celsius(float value) {
  if (isnan(value)) return "–";
  int rounded = (int)lroundf(value);
  return String(rounded == 0 ? 0 : rounded) + "°";
}

String fit(TFT_eSprite &spr, String text, int width) {
  if (spr.textWidth(text) <= width) return text;
  while (text.length() && spr.textWidth(text + "…") > width) {
    // Drop a whole UTF-8 character.
    int cut = text.length() - 1;
    while (cut > 0 && (text[cut] & 0xC0) == 0x80) cut--;
    text.remove(cut);
  }
  text.trim();
  return text + "…";
}

std::function<void(const uint16_t *, int)> paintCapture;

void paint(int top, int height, const std::function<void(TFT_eSprite &)> &draw) {
  // 40 rows of 480 pixels is 38 KB: room to spare next to Wi-Fi and TLS.
  const int STRIP = 40;
  TFT_eSprite spr(&tft);
  if (!spr.createSprite(WIDTH, min(STRIP, height))) return;
  spr.setTextWrap(false, false);
  for (int y = top; y < top + height; y += STRIP) {
    int h = min(STRIP, top + height - y);
    // Cleared first: with a viewport, fillSprite() clears the viewport's
    // whole height, far past the sprite's end.
    spr.fillSprite(BG);
    // Moves the sprite's origin, so `draw` can work in screen coordinates.
    spr.setViewport(0, -y, WIDTH, HEIGHT, true);
    draw(spr);
    spr.resetViewport();
    spr.pushSprite(0, y, 0, 0, WIDTH, h);
    if (paintCapture) paintCapture((const uint16_t *)spr.getPointer(), h);
  }
  spr.deleteSprite();
}

void drawHeader(TFT_eSprite &spr, const String &title, uint16_t color) {
  spr.loadFont(FONT_TEXT);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(color, BG);
  spr.drawString(title, 18, 22);
  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(TEXT, BG);
  spr.drawString(clockText(), WIDTH - 18, 22);
  spr.unloadFont();
}

void drawPageDots(TFT_eSprite &spr, int page, int pages) {
  const int gap = 14, y = HEIGHT - 9;
  int x = WIDTH / 2 - (pages - 1) * gap / 2;
  for (int i = 0; i < pages; i++) spr.fillSmoothCircle(x + i * gap, y, 3, i == page ? TEXT : DIM, BG);
}

void drawMessage(const String &title, const String &subtitle, uint16_t color) {
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.loadFont(FONT_VALUE);
  tft.setTextColor(color, BG);
  tft.drawString(title, WIDTH / 2, HEIGHT / 2 - 16);
  tft.unloadFont();
  tft.loadFont(FONT_TEXT);
  tft.setTextColor(MUTED, BG);
  tft.drawString(subtitle, WIDTH / 2, HEIGHT / 2 + 20);
  tft.unloadFont();
}

// MARK: - Weather codes

const char *conditionText(int code, bool day) {
  switch (code) {
    case 0: return day ? "Sonnig" : "Klar";
    case 1: return "Heiter";
    case 2: return "Wolkig";
    case 3: return "Bedeckt";
    case 45: case 48: return "Nebel";
    case 51: case 53: return "Nieselregen";
    case 55: return "Starker Niesel";
    case 56: case 57: case 66: case 67: return "Eisregen";
    case 61: return "Leichter Regen";
    case 63: return "Regen";
    case 65: return "Starker Regen";
    case 71: return "Leichter Schnee";
    case 73: return "Schnee";
    case 75: return "Starker Schnee";
    case 77: return "Schneegriesel";
    case 80: case 81: return "Regenschauer";
    case 82: return "Starke Schauer";
    case 85: case 86: return "Schneeschauer";
    case 95: return "Gewitter";
    case 96: case 99: return "Gewitter, Hagel";
    default: return "–";
  }
}

void drawWeatherIcon(TFT_eSprite &spr, const uint8_t *font, int code, bool day, int cx, int cy, uint16_t bg) {
  spr.loadFont(font);
  spr.setTextDatum(MC_DATUM);
  auto glyph = [&](const char *icon, uint16_t color, int x, int y) {
    spr.setTextColor(color, bg);
    spr.drawString(icon, x, y);
  };
  const char *sky = day ? ICON_SUN : ICON_MOON;
  uint16_t skyColor = day ? SUN : MOON;
  if (code == 1 || code == 2) {
    // The sun peeks out behind the cloud's top right; the filled cloud in the
    // background color hides the rays it covers.
    int size = spr.fontHeight(), dx = size * 18 / 100, dy = size * 16 / 100;
    glyph(sky, skyColor, cx + dx, cy - dy);
    int x = cx - size * 6 / 100, y = cy + size * 10 / 100;
    glyph(ICON_CLOUD_FILLED, bg, x, y);
    glyph(ICON_CLOUD, CLOUD, x, y);
  } else if (code == 0) {
    glyph(sky, skyColor, cx, cy);
  } else if (code == 3) {
    glyph(ICON_CLOUD, CLOUD, cx, cy);
  } else if (code == 45 || code == 48) {
    glyph(ICON_FOG, CLOUD, cx, cy);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    glyph(ICON_SNOW, SNOW, cx, cy);
  } else if (code >= 95) {
    glyph(ICON_STORM, STORM, cx, cy);
  } else if (code >= 51) {
    glyph(ICON_RAIN, RAIN, cx, cy);
  } else {
    glyph(ICON_CLOUD, DIM, cx, cy);
  }
  spr.unloadFont();
}

static uint16_t mix(uint16_t a, uint16_t b, float t) {
  t = constrain(t, 0.0f, 1.0f);
  return tft.alphaBlend((uint8_t)lroundf(t * 255), b, a);
}

uint16_t temperatureColor(float c) {
  struct Stop {
    float at;
    uint16_t color;
  };
  static const Stop STOPS[] = {
      {-10, rgb(110, 120, 240)}, {0, rgb(86, 170, 240)}, {8, rgb(80, 200, 190)}, {15, rgb(120, 205, 110)},
      {22, rgb(245, 200, 70)},   {28, rgb(245, 140, 50)}, {35, rgb(240, 80, 70)},
  };
  const int n = sizeof STOPS / sizeof STOPS[0];
  if (c <= STOPS[0].at) return STOPS[0].color;
  for (int i = 1; i < n; i++) {
    if (c <= STOPS[i].at) return mix(STOPS[i - 1].color, STOPS[i].color, (c - STOPS[i - 1].at) / (STOPS[i].at - STOPS[i - 1].at));
  }
  return STOPS[n - 1].color;
}
