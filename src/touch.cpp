// A minimal GT911 driver: the controller reports up to five points in a
// status register and a point buffer, which the host reads and then clears.
// Its native frame is the panel's portrait one, 320 x 480.

#include "touch.h"

#include <Arduino.h>
#include <Wire.h>

namespace touch {

namespace {

const int SDA_PIN = 33, SCL_PIN = 32, INT_PIN = 21, RST_PIN = 25;
const uint16_t STATUS = 0x814E, POINT = 0x8150;
uint8_t address = 0x5D;
bool found = false;

bool readRegister(uint16_t reg, uint8_t *data, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, (uint8_t)length) != length) return false;
  for (size_t i = 0; i < length; i++) data[i] = Wire.read();
  return true;
}

void writeRegister(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(value);
  Wire.endTransmission();
}

bool answers(uint8_t candidate) {
  Wire.beginTransmission(candidate);
  return Wire.endTransmission() == 0;
}

}  // namespace

bool begin() {
  // INT held low while RST rises selects address 0x5D.
  pinMode(RST_PIN, OUTPUT);
  pinMode(INT_PIN, OUTPUT);
  digitalWrite(INT_PIN, LOW);
  digitalWrite(RST_PIN, LOW);
  delay(10);
  digitalWrite(RST_PIN, HIGH);
  delay(10);
  pinMode(INT_PIN, INPUT);
  delay(50);

  Wire.begin(SDA_PIN, SCL_PIN, 400000);
  for (uint8_t candidate : {(uint8_t)0x5D, (uint8_t)0x14}) {
    if (answers(candidate)) {
      address = candidate;
      found = true;
      break;
    }
  }
  uint8_t id[4] = {0};
  if (found) readRegister(0x8140, id, 4);
  Serial.printf("touch: GT%c%c%c at 0x%02X%s\n", id[0], id[1], id[2], address, found ? "" : " not found");
  return found;
}

bool read(int &x, int &y) {
  if (!found) return false;
  uint8_t status;
  if (!readRegister(STATUS, &status, 1)) return false;
  static bool down = false;
  if (!(status & 0x80)) return down;  // nothing new since the last read
  int points = status & 0x0F;
  if (points > 0) {
    uint8_t p[4];
    if (readRegister(POINT, p, 4)) {
      int nx = p[0] | (p[1] << 8), ny = p[2] | (p[3] << 8);
      // Portrait to the landscape the display is turned to.
      x = constrain(ny, 0, 479);
      y = constrain(319 - nx, 0, 319);
    }
  }
  writeRegister(STATUS, 0);
  down = points > 0;
  return down;
}

}  // namespace touch
