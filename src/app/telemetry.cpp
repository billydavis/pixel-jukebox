#include "app/telemetry.h"

#include <esp_heap_caps.h>
#include <esp_system.h>

static const int MAX_TASKS = 6;

struct TaskRecord {
  const char *name;
  TaskHandle_t handle; // null for a short-lived task
  uint32_t stackBytes;
  uint32_t lowestFree; // for short-lived tasks, the smallest headroom seen
};

static TaskRecord tasks[MAX_TASKS];
static int taskCount = 0;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

static size_t largestImage = 0;
static uint32_t imageCount = 0;

static TaskRecord *find(const char *name) {
  for (int i = 0; i < taskCount; i++) {
    if (strcmp(tasks[i].name, name) == 0) {
      return &tasks[i];
    }
  }
  return nullptr;
}

void telemetryRegisterTask(const char *name, TaskHandle_t task, uint32_t stackBytes) {
  portENTER_CRITICAL(&lock);
  if (!find(name) && taskCount < MAX_TASKS) {
    tasks[taskCount++] = {name, task, stackBytes, stackBytes};
  }
  portEXIT_CRITICAL(&lock);
}

void telemetryNoteTaskExit(const char *name, uint32_t stackBytes) {
  uint32_t free = uxTaskGetStackHighWaterMark(nullptr);
  portENTER_CRITICAL(&lock);
  TaskRecord *record = find(name);
  if (!record && taskCount < MAX_TASKS) {
    tasks[taskCount] = {name, nullptr, stackBytes, stackBytes};
    record = &tasks[taskCount++];
  }
  if (record) {
    record->lowestFree = min(record->lowestFree, free);
  }
  portEXIT_CRITICAL(&lock);
}

void telemetryNoteImage(size_t bytes) {
  portENTER_CRITICAL(&lock);
  largestImage = max(largestImage, bytes);
  imageCount++;
  portEXIT_CRITICAL(&lock);
}

static const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
  }
}

static void reportRegion(JsonObject region, uint32_t caps) {
  region["free"] = heap_caps_get_free_size(caps);
  region["lowestFree"] = heap_caps_get_minimum_free_size(caps);
  region["largestBlock"] = heap_caps_get_largest_free_block(caps);
}

struct BootMark {
  const char *step;
  uint32_t ms;
};
static const int MAX_BOOT_MARKS = 16;
static BootMark bootMarks[MAX_BOOT_MARKS];
static int bootMarkCount = 0;

void telemetryBootMark(const char *step) {
  if (bootMarkCount < MAX_BOOT_MARKS) {
    bootMarks[bootMarkCount++] = {step, millis()};
  }
}

void telemetryReport(JsonObject report) {
  report["uptimeSeconds"] = millis() / 1000;
  report["lastReset"] = resetReason();
  JsonArray boot = report["boot"].to<JsonArray>();
  for (int i = 0; i < bootMarkCount; i++) {
    JsonObject mark = boot.add<JsonObject>();
    mark["step"] = bootMarks[i].step;
    mark["ms"] = bootMarks[i].ms;
  }

  reportRegion(report["heap"].to<JsonObject>(), MALLOC_CAP_INTERNAL);

  JsonObject psram = report["psram"].to<JsonObject>();
  psram["present"] = psramFound();
  psram["size"] = ESP.getPsramSize();
  reportRegion(psram, MALLOC_CAP_SPIRAM);

  // Copied out first: the stack query must not run inside the critical section.
  TaskRecord copy[MAX_TASKS];
  int count;
  portENTER_CRITICAL(&lock);
  count = taskCount;
  memcpy(copy, tasks, sizeof(tasks));
  size_t largest = largestImage;
  uint32_t images = imageCount;
  portEXIT_CRITICAL(&lock);

  JsonArray list = report["tasks"].to<JsonArray>();
  for (int i = 0; i < count; i++) {
    JsonObject item = list.add<JsonObject>();
    item["name"] = copy[i].name;
    item["stack"] = copy[i].stackBytes;
    item["lowestFree"] =
        copy[i].handle ? uxTaskGetStackHighWaterMark(copy[i].handle) : copy[i].lowestFree;
  }

  JsonObject artwork = report["artwork"].to<JsonObject>();
  artwork["downloaded"] = images;
  artwork["largestBytes"] = largest;
}
