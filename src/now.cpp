// The resting page, readable from across the room: the weather's icon and
// the temperature large, what it feels like and today's range beside them,
// and below a card that says when the rain starts or stops, with the next
// two hours as bars.

#include <math.h>

#include "config.h"
#include "screens.h"

namespace {

const int HERO_TOP = HEADER_HEIGHT, HERO_HEIGHT = 136;
const int CARD_X = 12, CARD_TOP = HERO_TOP + HERO_HEIGHT + 4, CARD_W = WIDTH - 2 * CARD_X, CARD_H = 122;
const int PAD = 18;
const int BARS = 24;  // two hours of five minutes

const Day *today() {
  time_t now = time(nullptr);
  for (int i = 0; i < dayCount; i++) {
    if (now >= days[i].date && now < days[i].date + DAY) return &days[i];
  }
  return nullptr;
}

void drawHero(TFT_eSprite &spr) {
  const int cy = HERO_TOP + HERO_HEIGHT / 2;
  if (!current.ok) {
    spr.loadFont(FONT_VALUE);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(MUTED, BG);
    spr.drawString("Keine Wetterdaten", WIDTH / 2, cy);
    spr.unloadFont();
    return;
  }
  drawWeatherIcon(spr, ICONS_LARGE, current.code, current.day, 84, cy, BG);

  // Details first: the temperature gets the room that's left.
  const Day *d = today();
  String lines[3] = {conditionText(current.code, current.day), "gefühlt " + celsius(current.feelsLike),
                     d ? "↓ " + celsius(d->min) + "   ↑ " + celsius(d->max) : ""};
  spr.loadFont(FONT_TEXT);
  spr.setTextDatum(MR_DATUM);
  int detailsWidth = 0;
  for (int i = 0; i < 3; i++) {
    spr.setTextColor(i == 0 ? TEXT : MUTED, BG);
    spr.drawString(lines[i], WIDTH - PAD, cy - 30 + i * 30);
    detailsWidth = max(detailsWidth, (int)spr.textWidth(lines[i]));
  }
  spr.unloadFont();

  const int left = 146, room = WIDTH - PAD - detailsWidth - 12 - left;
  spr.loadFont(FONT_TEMPERATURE);
  String temperature = decimal(current.temperature, 1) + "°";
  if (spr.textWidth(temperature) > room) temperature = celsius(current.temperature);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TEXT, BG);
  // The digits' visual middle sits a little below the font's.
  spr.drawString(temperature, left, cy + 6);
  spr.unloadFont();
}

void drawRainBars(TFT_eSprite &spr, int top, int height) {
  const int left = CARD_X + PAD, right = CARD_X + CARD_W - PAD, gap = 3;
  const float barW = (right - left + gap) / (float)BARS;
  const int base = top + height;
  time_t now = time(nullptr), slot = now / 300 * 300;
  for (int i = 0; i < BARS; i++) {
    time_t t = slot + i * 300;
    float v = rainAt(t + 150);
    int x = left + (int)lroundf(i * barW), w = (int)lroundf((i + 1) * barW) - (int)lroundf(i * barW) - gap;
    if (isnan(v) || v < WET_MMH) {
      spr.fillRect(x, base - 2, w, 2, isnan(v) ? TILE : DIM);
      continue;
    }
    // Square-root scale: drizzle still shows, a downpour doesn't dwarf it.
    int h = max(4, (int)lroundf(sqrtf(min(v, 10.0f) / 10.0f) * height));
    uint16_t color = fromRadar(t + 150) ? RAIN : rgb(58, 96, 150);
    spr.fillSmoothRoundRect(x, base - h, w, h, 2, color, TILE);
  }

  spr.loadFont(FONT_SMALL);
  spr.setTextColor(MUTED, TILE);
  spr.setTextDatum(TL_DATUM);
  spr.drawString("jetzt", left, base + 7);
  spr.setTextDatum(TC_DATUM);
  for (int i = 1; i < BARS; i++) {
    time_t t = slot + i * 300;
    int x = left + (int)lroundf(i * barW) - gap / 2;
    if ((t / 60) % 30 != 0 || x < left + 60 || x > right - 24) continue;
    spr.drawString(clockText(t), x, base + 7);
  }
  spr.unloadFont();
}

void drawRainCard(TFT_eSprite &spr) {
  spr.fillSmoothRoundRect(CARD_X, CARD_TOP, CARD_W, CARD_H, 12, TILE, BG);
  RainOutlook outlook = rainOutlook();
  const int x = CARD_X + PAD, width = CARD_W - 2 * PAD;
  spr.loadFont(FONT_VALUE);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(outlook.wet || outlook.soon ? RAIN : TEXT, TILE);
  spr.drawString(fit(spr, outlook.headline, width), x, CARD_TOP + 26);
  spr.unloadFont();
  spr.loadFont(FONT_TEXT);
  spr.setTextColor(MUTED, TILE);
  spr.drawString(fit(spr, outlook.detail, width), x, CARD_TOP + 54);
  spr.unloadFont();
  drawRainBars(spr, CARD_TOP + 68, 26);
}

}  // namespace

void drawNow() {
  paint(0, HEIGHT, [](TFT_eSprite &spr) {
    drawHeader(spr, dateText(), MUTED);
    drawHero(spr);
    drawRainCard(spr);
    drawPageDots(spr, NOW, PAGE_COUNT);
  });
}
