#include "app/clock_chip.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>

#include "config.h"
#include "pins.h"
#include "rtc_time.h"
#include "view_queue.h"

static const uint8_t ADDRESS = 0x51;
static const uint8_t REG_CONTROL_1 = 0x00, REG_SECONDS = 0x04;
static const uint8_t CONTROL_1_STOP = 0x20;  // set: the clock is stopped
static const uint8_t CONTROL_1_12_24 = 0x02; // set: 12-hour mode; the firmware uses 24
static const time_t TRUSTED_AFTER = 1700000000; // 2023: anything earlier was never set properly

static bool present = false;
static bool keptTime = false; // the chip had a valid time at start-up
static const char *source = "not set";
static volatile bool networkTimeArrived = false;
static time_t lastSync = 0;
static int32_t lastDrift = 0;
static bool haveDrift = false;

static bool readRegisters(uint8_t reg, uint8_t *out, size_t length) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDRESS, (uint8_t)length) != length) return false;
  for (size_t i = 0; i < length; i++) out[i] = Wire.read();
  return true;
}

static bool writeRegisters(uint8_t reg, const uint8_t *values, size_t length) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  Wire.write(values, length);
  return Wire.endTransmission() == 0;
}

// Runs in the SNTP task; the chip is written later from the display loop.
static void onNetworkTime(struct timeval *) { networkTimeArrived = true; }

void clockChipBegin() {
  // Without this the local time is only right once Wi-Fi has connected.
  setenv("TZ", TIMEZONE, 1);
  tzset();
  sntp_set_time_sync_notification_cb(onNetworkTime);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  uint8_t control;
  if (!readRegisters(REG_CONTROL_1, &control, 1)) {
    Serial.println("Clock chip: not found");
    return;
  }
  present = true;
  if (control & (CONTROL_1_STOP | CONTROL_1_12_24)) {
    uint8_t running = control & ~(CONTROL_1_STOP | CONTROL_1_12_24);
    writeRegisters(REG_CONTROL_1, &running, 1);
  }

  uint8_t regs[rtc::REGISTERS];
  uint32_t chipTime;
  if (!readRegisters(REG_SECONDS, regs, sizeof(regs)) || !rtc::decode(regs, chipTime) ||
      (time_t)chipTime < TRUSTED_AFTER) {
    Serial.println("Clock chip: no time kept (the power was off); waiting for the network");
    return;
  }
  struct timeval now = {(time_t)chipTime, 0};
  settimeofday(&now, nullptr);
  keptTime = true;
  source = "clock chip";
  struct tm local;
  localtime_r(&now.tv_sec, &local);
  Serial.printf("Clock chip: time set to %04d-%02d-%02d %02d:%02d:%02d local\n", local.tm_year + 1900,
                local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

void clockChipMaintain() {
  if (!networkTimeArrived) {
    return;
  }
  networkTimeArrived = false;
  source = "network";
  if (!present) {
    return;
  }
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  uint32_t now = tv.tv_sec + (tv.tv_usec >= 500000 ? 1 : 0); // the chip counts whole seconds

  // How far the chip had wandered since it was last set, for the diagnostics.
  uint8_t regs[rtc::REGISTERS];
  uint32_t chipTime;
  if (readRegisters(REG_SECONDS, regs, sizeof(regs)) && rtc::decode(regs, chipTime)) {
    lastDrift = (int32_t)(chipTime - now);
    haveDrift = true;
  }
  rtc::encode(now, regs);
  if (writeRegisters(REG_SECONDS, regs, sizeof(regs))) {
    lastSync = now;
    if (haveDrift) {
      Serial.printf("Clock chip: set from the network (it was %+ld s out)\n", (long)lastDrift);
    } else {
      Serial.println("Clock chip: set from the network");
    }
  }
}

void clockReport(JsonObject report) {
  time_t now = time(nullptr);
  report["source"] = source;
  if (now >= TRUSTED_AFTER) {
    report["utc"] = views::isoTime((uint32_t)now);
  } else {
    report["utc"] = nullptr;
  }
  JsonObject chip = report["chip"].to<JsonObject>();
  chip["present"] = present;
  chip["keptTimeAtStart"] = keptTime;
  if (lastSync) {
    chip["lastSetUtc"] = views::isoTime((uint32_t)lastSync);
  }
  if (haveDrift) {
    chip["driftSecondsAtLastSet"] = lastDrift;
  }
}
