// The next seven days, a row each: the day, its weather, the chance and
// amount of rain, and the low and high on one scale for the whole week, so
// the warm and cold days stand out at a glance.

#include <math.h>

#include "screens.h"

namespace {

const int ROW_TOP = HEADER_HEIGHT, ROW_H = 38;
const int DAY_X = 18, ICON_X = 140, RAIN_X = 170;
const int MIN_X = 340, BAR_X = 350, BAR_W = 74, MAX_X = WIDTH - 18;

String dayName(const Day &d, int index) {
  static const char *DAYS[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
  if (index == 0) return "Heute";
  if (index == 1) return "Morgen";
  struct tm local;
  localtime_r(&d.date, &local);
  return DAYS[local.tm_wday];
}

void drawRow(TFT_eSprite &spr, const Day &d, int index, float low, float high) {
  const int top = ROW_TOP + index * ROW_H, cy = top + ROW_H / 2;
  if (index > 0) spr.drawFastHLine(DAY_X, top, WIDTH - 2 * DAY_X, LINE);

  spr.loadFont(FONT_TEXT);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TEXT, BG);
  spr.drawString(dayName(d, index), DAY_X, cy);
  spr.unloadFont();

  drawWeatherIcon(spr, ICONS_MEDIUM, d.code, true, ICON_X, cy, BG);

  // Rain only when it's worth a look.
  bool likely = d.rainChance >= 30 || d.rain >= 1;
  if (d.rainChance >= 10 || d.rain >= 0.2f) {
    spr.loadFont(ICONS_SMALL);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(likely ? RAIN : MUTED, BG);
    spr.drawString(ICON_DROPLET, RAIN_X, cy);
    spr.unloadFont();
    String text = String(d.rainChance) + " %";
    if (d.rain >= 0.2f) text += " · " + decimal(d.rain, d.rain < 10 ? 1 : 0) + " mm";
    spr.loadFont(FONT_SMALL);
    spr.setTextColor(likely ? TEXT : MUTED, BG);
    spr.drawString(text, RAIN_X + 22, cy + 1);
    spr.unloadFont();
  }

  spr.loadFont(FONT_TEXT);
  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(MUTED, BG);
  spr.drawString(celsius(d.min), MIN_X, cy);
  spr.setTextColor(TEXT, BG);
  spr.drawString(celsius(d.max), MAX_X, cy);
  spr.unloadFont();

  // The day's range on the week's scale, colored by temperature.
  const int barH = 6, y = cy - barH / 2;
  spr.fillSmoothRoundRect(BAR_X, y, BAR_W, barH, barH / 2, LINE, BG);
  if (isnan(d.min) || isnan(d.max) || high <= low) return;
  auto xOf = [&](float t) { return BAR_X + (int)lroundf((t - low) / (high - low) * (BAR_W - barH)); };
  // Round caps in the end colors, a gradient between them.
  const int r = barH / 2;
  int x0 = xOf(d.min) + r, x1 = xOf(d.max) + r;
  spr.fillSmoothCircle(x0, cy, r, temperatureColor(d.min), LINE);
  spr.fillSmoothCircle(x1, cy, r, temperatureColor(d.max), LINE);
  for (int x = x0; x <= x1; x++) {
    float t = low + (x - r - BAR_X) / (float)(BAR_W - barH) * (high - low);
    spr.drawFastVLine(x, y, barH, temperatureColor(t));
  }
  // Today: where the temperature is now.
  if (index == 0 && current.ok && current.temperature >= d.min && current.temperature <= d.max) {
    spr.fillSmoothCircle(xOf(current.temperature) + barH / 2, cy, 5, BG, BG);
    spr.fillSmoothCircle(xOf(current.temperature) + barH / 2, cy, 3, TEXT, BG);
  }
}

}  // namespace

void drawWeek() {
  float low = INFINITY, high = -INFINITY;
  for (int i = 0; i < dayCount; i++) {
    low = min(low, days[i].min);
    high = max(high, days[i].max);
  }
  // Today's row starts at the first day that hasn't ended yet.
  int first = 0;
  time_t now = time(nullptr);
  while (first < dayCount - 1 && days[first].date + DAY <= now) first++;
  paint(0, HEIGHT, [&](TFT_eSprite &spr) {
    drawHeader(spr, "Die nächsten 7 Tage", TEXT);
    if (dayCount == 0) {
      spr.loadFont(FONT_VALUE);
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(MUTED, BG);
      spr.drawString("Keine Vorhersage", WIDTH / 2, HEIGHT / 2);
      spr.unloadFont();
    }
    for (int i = first; i < dayCount; i++) drawRow(spr, days[i], i - first, low, high);
    drawPageDots(spr, WEEK, PAGE_COUNT);
  });
}
