#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <functional>

#include "fonts.h"

extern TFT_eSPI tft;

const int WIDTH = 480, HEIGHT = 320;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Black, with warm grays.
const uint16_t BG = rgb(0, 0, 0);
const uint16_t TILE = rgb(26, 25, 23);
const uint16_t LINE = rgb(48, 46, 42);
const uint16_t TEXT = rgb(245, 244, 238);
const uint16_t MUTED = rgb(150, 146, 137);
const uint16_t DIM = rgb(58, 56, 51);
const uint16_t ORANGE = rgb(245, 165, 36);
const uint16_t SUN = rgb(245, 196, 66);
const uint16_t MOON = rgb(200, 204, 222);
const uint16_t CLOUD = rgb(178, 180, 186);
const uint16_t RAIN = rgb(86, 156, 240);
const uint16_t SNOW = rgb(226, 238, 248);
const uint16_t STORM = rgb(176, 132, 240);

const int HEADER_HEIGHT = 40;
const long DAY = 86400;

bool clockSet();
/** Local "HH:MM", or "" before NTP has answered. */
String clockText();
String clockText(time_t at);
/** "Dienstag, 3. November". */
String dateText();
/** Unix time from an ISO 8601 timestamp with offset, as Bright Sky sends. */
time_t parseIso(const String &value);
/** `value` with a decimal comma. */
String decimal(float value, int decimals);
/** "9°", rounded. */
String celsius(float value);

/** Shortens `text` with "…" until it fits `width` in the sprite's font. */
String fit(TFT_eSprite &spr, String text, int width);

/** Draws a full-width band of the screen through a sprite small enough for
 * the heap, a strip at a time. `draw` uses screen coordinates. */
void paint(int top, int height, const std::function<void(TFT_eSprite &)> &draw);
/** When set, also gets every strip paint() draws, top to bottom: `rows`
 * full-width rows of the sprite's (byte-swapped) pixels. For screenshots. */
extern std::function<void(const uint16_t *pixels, int rows)> paintCapture;

/** The top row: `title` on the left, the clock on the right. */
void drawHeader(TFT_eSprite &spr, const String &title, uint16_t color);
/** Two dots at the bottom, the current page's lit. */
void drawPageDots(TFT_eSprite &spr, int page, int pages);
void drawMessage(const String &title, const String &subtitle, uint16_t color);

// MARK: - Weather codes (WMO, as Open-Meteo sends them)

/** "Sonnig", "Bedeckt", "Regenschauer". */
const char *conditionText(int code, bool day);
/** The code's icon centered at (cx, cy), in a font from fonts.h. Partly
 * cloudy is a sun or moon behind a cloud. */
void drawWeatherIcon(TFT_eSprite &spr, const uint8_t *font, int code, bool day, int cx, int cy, uint16_t bg);
/** From blue in the frost to red in the heat. */
uint16_t temperatureColor(float celsius);
