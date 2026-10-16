// Adapted from p64 (https://github.com/fabkury/p64),
// firmware/components/p64_inputs/src/qmi8658.cpp.
// Copyright 2026 Fabrício Kury. Licensed under the Apache License 2.0.
// Changes: uses the Arduino Wire library instead of the ESP-IDF I2C driver.

#include "input/motion.h"

#include <Arduino.h>
#include <Wire.h>

#include "pins.h"

static const uint8_t REG_WHO_AM_I = 0x00, REG_REVISION = 0x01, REG_CTRL1 = 0x02,
                     REG_CTRL2 = 0x03, REG_CTRL3 = 0x04, REG_CTRL5 = 0x06, REG_CTRL7 = 0x08,
                     REG_AX_L = 0x35, REG_RESET = 0x60;
static const uint8_t CHIP_ID = 0x05;
static const float LSB_PER_G = 4096.0f; // +-8 g full scale, 16 bit

static uint8_t address = 0;

static bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool readRegisters(uint8_t reg, uint8_t *out, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, (uint8_t)length) != length) return false;
  for (size_t i = 0; i < length; i++) out[i] = Wire.read();
  return true;
}

static bool probe(uint8_t candidate) {
  address = candidate;
  uint8_t id = 0;
  if (readRegisters(REG_WHO_AM_I, &id, 1) && id == CHIP_ID) return true;
  address = 0;
  return false;
}

bool motionBegin() {
  if (address) return true;
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  if (!probe(0x6B) && !probe(0x6A)) {
    Serial.println("Motion sensor: no QMI8658 on the I2C bus");
    return false;
  }
  writeRegister(REG_RESET, 0xB0);
  delay(20);
  uint8_t revision = 0;
  readRegisters(REG_REVISION, &revision, 1);
  bool ok = writeRegister(REG_CTRL1, 0x40);       // address auto-increment, little-endian
  ok = writeRegister(REG_CTRL2, 0x24) && ok;      // accelerometer +-8 g, 500 Hz
  ok = writeRegister(REG_CTRL3, 0x00) && ok;      // gyroscope off
  ok = writeRegister(REG_CTRL5, 0x00) && ok;      // no low-pass filter: taps are short
  ok = writeRegister(REG_CTRL7, 0x01) && ok;      // accelerometer on
  delay(10);
  float ax, ay, az;
  if (!ok || !motionRead(ax, ay, az)) {
    Serial.println("Motion sensor: QMI8658 found but not answering");
    address = 0;
    return false;
  }
  Serial.printf("Motion sensor: QMI8658 at 0x%02X, revision 0x%02X, %.2f %.2f %.2f g\n", address,
                revision, ax, ay, az);
  return true;
}

bool motionPresent() { return address != 0; }

uint8_t motionAddress() { return address; }

bool motionRead(float &ax, float &ay, float &az) {
  if (!address) return false;
  uint8_t raw[6];
  if (!readRegisters(REG_AX_L, raw, sizeof(raw))) return false;
  ax = (int16_t)(raw[0] | (raw[1] << 8)) / LSB_PER_G;
  ay = (int16_t)(raw[2] | (raw[3] << 8)) / LSB_PER_G;
  az = (int16_t)(raw[4] | (raw[5] << 8)) / LSB_PER_G;
  return true;
}
