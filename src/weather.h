#pragma once

#include <Arduino.h>
#include <time.h>

/** Right now, from Open-Meteo. */
struct Current {
  bool ok = false;
  float temperature = NAN, feelsLike = NAN;
  int code = -1;
  bool day = true;
};

struct Day {
  time_t date;  // local midnight
  int code;
  float min, max, rain;  // °C, °C, mm
  int rainChance;        // %
};

/** Precipitation in mm/h, in steps of `step` seconds. Each value is the
 * amount that falls in the `step` seconds before its time. */
struct Series {
  static const int CAPACITY = 64;
  time_t first = 0;
  int step = 300, count = 0;
  float mmh[CAPACITY];
  /** NAN outside the series. */
  float at(time_t t) const;
  time_t end() const { return first + (count - 1) * step; }
};

extern Current current;
extern Day days[7];
extern int dayCount;
/** DWD radar, 5 minutes, about two hours ahead (Bright Sky). */
extern Series radar;
/** Open-Meteo's model, 15 minutes, twelve hours ahead. */
extern Series model;
extern time_t forecastAt, radarAt;

/** Current conditions, rain for the next 12 hours, and the week. */
bool refreshForecast();
/** Rain radar for the next two hours. */
bool refreshRadar();

/** Precipitation at `t` in mm/h: from the radar while it reaches, then the
 * model. NAN when neither does. */
float rainAt(time_t t);
/** Whether `t` falls in the radar's (fresh) part. */
bool fromRadar(time_t t);

// MARK: - What the rain will do

/** Below this, it's dry. */
const float WET_MMH = 0.2f;

struct RainOutlook {
  String headline;  // "Es beginnt in 25 Minuten zu regnen"
  String detail;    // "Leichter Regen, etwa 40 Minuten lang"
  bool wet;         // raining now
  bool soon;        // starts within the radar's reach
};
RainOutlook rainOutlook();
/** "Leichter Regen", "Starker Schnee". */
String intensityText(float mmh);
