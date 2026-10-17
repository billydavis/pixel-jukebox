#include "input/case_climate.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "app/memory_trace.h"
#include "app/telemetry.h"
#include "pins.h"
#include "shtc3_math.h"

static const uint8_t ADDRESS = 0x70;
static const uint16_t CMD_WAKE = 0x3517, CMD_SLEEP = 0xB098, CMD_READ_ID = 0xEFC8;
static const uint16_t CMD_MEASURE = 0x7866; // normal power, temperature first, no clock stretching
static const uint32_t MEASURE_MS = 13;      // 12.1 ms at most
static const uint32_t PERIOD_MS = 60000;
static const uint32_t STACK_BYTES = 3072;

static SemaphoreHandle_t lock;
static bool present = false;
static bool haveReading = false;
static float latestC = NAN, latestRh = NAN, lowestC = NAN, highestC = NAN;
static uint32_t readAt = 0;
static uint32_t failures = 0;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static bool command(uint16_t code) {
  Wire.beginTransmission(ADDRESS);
  Wire.write((uint8_t)(code >> 8));
  Wire.write((uint8_t)code);
  return Wire.endTransmission() == 0;
}

static bool readBytes(uint8_t *out, size_t length) {
  if (Wire.requestFrom(ADDRESS, (uint8_t)length) != length) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    out[i] = Wire.read();
  }
  return true;
}

static bool wake() {
  bool ok = command(CMD_WAKE);
  delayMicroseconds(300); // 240 us to wake
  return ok;
}

static bool measure(float &celsius, float &humidity) {
  if (!wake() || !command(CMD_MEASURE)) {
    return false;
  }
  vTaskDelay(pdMS_TO_TICKS(MEASURE_MS));
  uint8_t bytes[6];
  bool ok = readBytes(bytes, sizeof(bytes)) && shtc3::decode(bytes, celsius, humidity);
  command(CMD_SLEEP);
  return ok;
}

static void climateTask(void *) {
  for (;;) {
    float celsius, humidity;
    bool measured = measure(celsius, humidity);
    MEMORY_CHECK("climate");
    if (measured) {
      Guard guard;
      latestC = celsius;
      latestRh = humidity;
      lowestC = haveReading ? fminf(lowestC, celsius) : celsius;
      highestC = haveReading ? fmaxf(highestC, celsius) : celsius;
      haveReading = true;
      readAt = millis();
    } else {
      Guard guard;
      failures++;
    }
    vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
  }
}

void caseClimateBegin() {
  lock = xSemaphoreCreateMutex();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
  uint8_t id[3];
  if (!wake() || !command(CMD_READ_ID) || !readBytes(id, sizeof(id)) || shtc3::crc(id, 2) != id[2] ||
      !shtc3::isShtc3((uint16_t)(id[0] << 8 | id[1]))) {
    Serial.println("Case sensor: no SHTC3 found");
    return;
  }
  command(CMD_SLEEP);
  present = true;
  TaskHandle_t task = nullptr;
  xTaskCreatePinnedToCore(climateTask, "climate", STACK_BYTES, nullptr, 1, &task, 0);
  telemetryRegisterTask("climate", task, STACK_BYTES);
}

static float tenths(float value) { return roundf(value * 10.0f) / 10.0f; }

void caseClimateReport(JsonObject report) {
  report["present"] = present;
  if (!present) {
    return;
  }
  Guard guard;
  if (!haveReading) {
    report["temperatureC"] = nullptr;
    report["humidityPercent"] = nullptr;
  } else {
    report["temperatureC"] = tenths(latestC);
    report["humidityPercent"] = tenths(latestRh);
    report["lowestC"] = tenths(lowestC);
    report["highestC"] = tenths(highestC);
    report["secondsAgo"] = (millis() - readAt) / 1000;
  }
  if (failures) {
    report["failedReads"] = failures;
  }
}
