#include "app/crash_report.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_core_dump.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "config.h"
#include "crash_cause.h"
#include "view_queue.h"

static const time_t TRUSTED_AFTER = 1700000000; // before 2023, the clock was not set

static Preferences prefs;
static const esp_partition_t *partition = nullptr;
static bool present = false;
static size_t imageOffset = 0; // within the partition
static size_t imageSize = 0;
static bool haveSummary = false;
static esp_core_dump_summary_t summary;
static bool matchesRunning = false;

static String hex(uint32_t value) {
  char text[11];
  snprintf(text, sizeof(text), "0x%08lx", (unsigned long)value);
  return text;
}

static const char *resetName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_PANIC:
      return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SW:
      return "software restart";
    case ESP_RST_POWERON:
      return "power on";
    default:
      return "other";
  }
}

void crashBegin() {
  prefs.begin("crash", false);
  partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  size_t address, size;
  if (!partition || esp_core_dump_image_check() != ESP_OK || esp_core_dump_image_get(&address, &size) != ESP_OK ||
      address < partition->address || size < 8) {
    return;
  }
  present = true;
  imageOffset = address - partition->address;
  imageSize = size;
  haveSummary = esp_core_dump_get_summary(&summary) == ESP_OK;

  // The record ends with its checksum: a different one means a new crash,
  // so note when it was found and what was running.
  uint32_t checksum = 0;
  esp_partition_read(partition, imageOffset + imageSize - 4, &checksum, sizeof(checksum));
  if (checksum != prefs.getUInt("sum", 0) || imageSize != prefs.getUInt("size", 0)) {
    time_t now = time(nullptr);
    prefs.putUInt("sum", checksum);
    prefs.putUInt("size", imageSize);
    prefs.putUInt("found", now > TRUSTED_AFTER ? (uint32_t)now : 0);
    prefs.putString("firmware", FIRMWARE_VERSION);
    prefs.putString("reset", resetName(esp_reset_reason()));
  }

  char running[65] = {0};
  esp_ota_get_app_elf_sha256(running, sizeof(running));
  const char *crashed = (const char *)summary.app_elf_sha256;
  size_t length = haveSummary ? strnlen(crashed, sizeof(summary.app_elf_sha256)) : 0;
  matchesRunning = length > 0 && strncmp(running, crashed, length) == 0;

  if (haveSummary) {
    Serial.printf("Crash record: task %s, %s at %s%s\n", summary.exc_task,
                  crash::causeName(summary.ex_info.exc_cause), hex(summary.exc_pc).c_str(),
                  matchesRunning ? "" : " (an earlier firmware)");
  } else {
    Serial.println("Crash record: present, but its summary cannot be read");
  }
}

bool crashPresent() { return present; }

void crashReport(JsonObject report) {
  report["present"] = present;
  if (!present) {
    return;
  }
  report["dumpBytes"] = imageSize;
  uint32_t found = prefs.getUInt("found", 0);
  if (found) {
    report["foundUtc"] = views::isoTime(found);
  } else {
    report["foundUtc"] = nullptr;
  }
  report["firmware"] = prefs.getString("firmware", "");
  report["resetReason"] = prefs.getString("reset", "");
  report["matchesRunningFirmware"] = matchesRunning;
  if (!haveSummary) {
    return;
  }
  report["task"] = summary.exc_task;
  uint32_t cause = summary.ex_info.exc_cause;
  report["cause"] = crash::causeName(cause);
  report["causeCode"] = cause;
  if (const char *hint = crash::causeHint(cause)) {
    report["hint"] = hint;
  }
  report["pc"] = hex(summary.exc_pc);
  report["address"] = hex(summary.ex_info.exc_vaddr);
  JsonArray backtrace = report["backtrace"].to<JsonArray>();
  for (uint32_t i = 0; i < summary.exc_bt_info.depth && i < 16; i++) {
    backtrace.add(hex(summary.exc_bt_info.bt[i]));
  }
  report["backtraceCorrupted"] = summary.exc_bt_info.corrupted;
  report["elfSha256"] = (const char *)summary.app_elf_sha256;
}

void crashBrief(JsonObject report) {
  report["present"] = present;
  if (present && haveSummary) {
    report["task"] = summary.exc_task;
    report["cause"] = crash::causeName(summary.ex_info.exc_cause);
  }
}

size_t crashDumpSize() { return present ? imageSize : 0; }

bool crashDumpRead(size_t offset, uint8_t *buffer, size_t length) {
  if (!present || offset + length > imageSize) {
    return false;
  }
  return esp_partition_read(partition, imageOffset + offset, buffer, length) == ESP_OK;
}

void crashErase() {
  if (present) {
    esp_core_dump_image_erase();
  }
  present = false;
  haveSummary = false;
  prefs.clear();
}
