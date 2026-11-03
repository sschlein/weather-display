// A weather display for a Cheap Yellow Display with a 3.5" screen
// (ESP32-3248S035C), for the hallway or the kitchen.
//
// Page 1 rests on the temperature and what the rain will do: "Es beginnt in
// 25 Minuten zu regnen", from the DWD's radar. Page 2 is the next seven
// days. A tap switches pages; page 2 goes back to page 1 after a minute.
//
// It answers at http://wetter-display.local: GET /screen.bmp is a
// screenshot, POST /tap?x=…&y=… a tap without a finger. Over USB,
// tools/screenshot.py does the same.
//
// Hold a finger on the screen while powering on to forget the Wi-Fi network.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

#include "config.h"
#include "screens.h"
#include "touch.h"

const char *HOSTNAME = "wetter-display";
const char *SETUP_AP = "Wetter-Display";
const char *TIMEZONE = "CET-1CEST,M3.5.0,M10.5.0/3";
const uint32_t FORECAST_MS = 10 * 60000, RADAR_MS = 5 * 60000, RETRY_MS = 60000;
const uint32_t WEEK_MS = 60000;
const int LED_RED = 4, LED_GREEN = 16, LED_BLUE = 17;

// Inflating the radar's frames takes 11 KB of stack; the default is 8 KB.
SET_LOOP_TASK_STACK_SIZE(24 * 1024);

TFT_eSPI tft;
WebServer server(80);

Page page = NOW;
uint32_t pageOpenedAt = 0;
uint32_t forecastTriedAt = 0, radarTriedAt = 0;
bool forecastOk = false, radarOk = false;

void render() {
  if (page == WEEK) {
    drawWeek();
  } else {
    drawNow();
  }
}

void onTap(int x, int y) {
  page = page == NOW ? WEEK : NOW;
  pageOpenedAt = millis();
  render();
}

void handleTouch() {
  static bool wasDown = false;
  int x = 0, y = 0;
  bool down = touch::read(x, y);
  if (down && !wasDown) {
    Serial.printf("tap %d,%d\n", x, y);
    onTap(x, y);
  }
  wasDown = down;
}

// MARK: - Debugging

const uint32_t SCREENSHOT_SIZE = 54 + WIDTH * 3 * HEIGHT;

/** The current page as a BMP. This panel's MISO line isn't wired, so it
 * can't be read back: the page is drawn again and its strips are sent as
 * they go to the panel. */
void writeScreenshot(const std::function<void(const uint8_t *, size_t)> &write) {
  const int w = WIDTH, h = HEIGHT;
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) { memcpy(header + at, &v, 4); };
  put32(2, SCREENSHOT_SIZE);
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, (uint32_t)-h);  // negative: rows top-down, as they're drawn
  header[26] = 1;
  header[28] = 24;
  put32(34, w * 3 * h);
  write(header, sizeof header);
  paintCapture = [&](const uint16_t *pixels, int rows) {
    static uint8_t row[WIDTH * 3];
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < w; x++) {
        uint16_t c = pixels[y * w + x];
        c = (c >> 8) | (c << 8);  // sprites keep their pixels byte-swapped
        row[x * 3] = (c & 0x1F) << 3;
        row[x * 3 + 1] = ((c >> 5) & 0x3F) << 2;
        row[x * 3 + 2] = (c >> 11) << 3;
      }
      write(row, sizeof row);
    }
  };
  render();
  paintCapture = nullptr;
}

/** GET /screen.bmp. */
void sendScreenshot() {
  server.setContentLength(SCREENSHOT_SIZE);
  server.send(200, "image/bmp", "");
  writeScreenshot([](const uint8_t *data, size_t length) { server.sendContent((const char *)data, length); });
}

/** Over USB, for networks that keep devices apart: "s" answers a line
 * "BMP <size>" and the screenshot, "t" taps. */
void handleSerial() {
  if (!Serial.available()) return;
  char command = Serial.read();
  if (command == 's') {
    Serial.printf("BMP %u\n", SCREENSHOT_SIZE);
    writeScreenshot([](const uint8_t *data, size_t length) { Serial.write(data, length); });
  } else if (command == 'r' || command == 'R') {
    // Made-up radar until the next real one, to see the rain card at work:
    // "r" rain in 25 minutes for 40, "R" rain now that stops in 20.
    time_t now = time(nullptr) / 300 * 300;
    radar.first = now;
    radar.step = 300;
    radar.count = 25;
    for (int i = 0; i < radar.count; i++) {
      bool wet = command == 'r' ? (i >= 6 && i < 14) : i < 5;
      radar.mmh[i] = wet ? 0.6f + 3.0f * sinf((command == 'r' ? i - 6 : i + 2) * 0.4f) : 0;
    }
    radarAt = time(nullptr);
    Serial.println("demo rain");
    render();
  } else if (command == 't') {
    Serial.println("tap over USB");
    onTap(WIDTH / 2, HEIGHT / 2);
  }
}

// MARK: - Setup and loop

void connectWifi(bool reset) {
  WiFiManager wm;
  if (reset) wm.resetSettings();
  wm.setHostname(HOSTNAME);
  wm.setConnectTimeout(20);
  wm.setAPCallback([](WiFiManager *) {
    drawMessage("WLAN einrichten", String("Mit \"") + SETUP_AP + "\" verbinden", TEXT);
  });
  wm.setConfigPortalTimeout(0);
  wm.autoConnect(SETUP_AP);
}

void setup() {
  Serial.begin(460800);
  // The RGB LED is active low: keep it dark.
  for (int pin : {LED_RED, LED_GREEN, LED_BLUE}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG);
  touch::begin();

  int x, y;
  delay(100);
  bool reset = touch::read(x, y);
  drawMessage(reset ? "WLAN vergessen" : "Verbinde…", reset ? "Loslassen zum Fortfahren" : "", reset ? ORANGE : TEXT);
  connectWifi(reset);
  // Modem sleep adds ~250 ms to every request.
  WiFi.setSleep(false);
  configTzTime(TIMEZONE, "pool.ntp.org", "time.cloudflare.com");
  for (int i = 0; i < 50 && !clockSet(); i++) delay(100);

  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  server.on("/screen.bmp", HTTP_GET, sendScreenshot);
  server.on("/tap", HTTP_POST, [] {
    server.send(204);
    onTap(server.arg("x").toInt(), server.arg("y").toInt());
  });
  server.begin();

  drawMessage("Lade das Wetter…", PLACE, TEXT);
  forecastOk = refreshForecast();
  radarOk = refreshRadar();
  forecastTriedAt = radarTriedAt = millis();
  Serial.printf("heap: %u free, %u largest block\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  render();
}

void loop() {
  server.handleClient();
  handleTouch();
  handleSerial();
  uint32_t now = millis();
  bool changed = false;

  if (now - forecastTriedAt > (forecastOk ? FORECAST_MS : RETRY_MS)) {
    forecastOk = refreshForecast();
    forecastTriedAt = millis();
    changed = true;
  }
  if (now - radarTriedAt > (radarOk ? RADAR_MS : RETRY_MS)) {
    radarOk = refreshRadar();
    radarTriedAt = millis();
    changed = true;
  }
  if (page != NOW && now - pageOpenedAt > WEEK_MS) {
    page = NOW;
    changed = true;
  }

  // The clock, and "in 25 Minuten" counting down.
  static int lastMinute = -1;
  int minute = (time(nullptr) / 60) % 60;
  if (minute != lastMinute) {
    lastMinute = minute;
    changed = true;
  }
  if (changed) render();

  static uint32_t lastReconnect = 0;
  if (WiFi.status() != WL_CONNECTED && now - lastReconnect > 30000) {
    lastReconnect = now;
    WiFi.reconnect();
  }
  delay(10);
}
