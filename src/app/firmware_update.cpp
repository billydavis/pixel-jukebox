#include "app/firmware_update.h"

#include <Update.h>
#include <esp_ota_ops.h>

#include "config.h"

// How long a new firmware must have its API up before it is kept, and how long
// it gets to reach that point. With the router down the old firmware comes
// back after the limit, which is harmless: it was working.
static const uint32_t KEEP_AFTER_API_MS = 30000;
static const uint32_t PROBATION_LIMIT_MS = 10UL * 60 * 1000;

static volatile bool active = false;
static size_t expected = 0;
static bool onProbation = false;
static uint32_t apiUpSince = 0;

// Arduino keeps every start-up unless told otherwise; this tells it that the
// sketch decides (see firmwareUpdateMaintain).
extern "C" bool verifyRollbackLater() { return true; }

void firmwareUpdateBegin() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  onProbation = esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  Serial.printf("Firmware: %s in %s%s\n", FIRMWARE_VERSION, running->label,
                onProbation ? ", new: kept once the API has been up for 30 s" : "");
#ifdef OTA_ROLLBACK_TEST
  // A deliberately broken update, to prove the way back works: build with
  // -DOTA_ROLLBACK_TEST and send it over Wi-Fi. It crashes only on probation,
  // so the same build put on over USB runs normally.
  if (onProbation) {
    Serial.println("Firmware: OTA_ROLLBACK_TEST, crashing on purpose");
    Serial.flush();
    abort();
  }
#endif
}

void firmwareUpdateMaintain(bool apiUp) {
  if (!onProbation) {
    return;
  }
  uint32_t now = millis();
  if (!apiUp) {
    apiUpSince = 0;
  } else if (apiUpSince == 0) {
    apiUpSince = now;
  }
  if (apiUpSince != 0 && now - apiUpSince >= KEEP_AFTER_API_MS) {
    esp_ota_mark_app_valid_cancel_rollback();
    onProbation = false;
    Serial.println("Firmware: new firmware kept");
  } else if (now >= PROBATION_LIMIT_MS) {
    Serial.println("Firmware: the API never came up; going back to the previous firmware");
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
}

bool firmwareUpdateStart(size_t expectedBytes, const String &md5, String &error) {
  if (active) {
    error = "An update is already in progress";
    return false;
  }
  if (onProbation) {
    // Overwriting the old firmware now would leave nothing to go back to.
    error = "The last update is still being checked; try again in a minute";
    return false;
  }
  if (!Update.begin(expectedBytes > 0 ? expectedBytes : UPDATE_SIZE_UNKNOWN)) {
    error = String("Cannot start the update: ") + Update.errorString();
    return false;
  }
  if (!md5.isEmpty() && !Update.setMD5(md5.c_str())) {
    Update.abort();
    error = "X-Firmware-MD5 must be 32 hex digits";
    return false;
  }
  expected = expectedBytes;
  active = true;
  Serial.printf("Firmware: receiving %u bytes\n", (unsigned)expectedBytes);
  return true;
}

bool firmwareUpdateWrite(uint8_t *data, size_t length, String &error) {
  if (!active) {
    return false;
  }
  if (Update.write(data, length) != length) {
    error = Update.getError() == UPDATE_ERROR_MAGIC_BYTE
                ? "That is not a firmware file; send firmware.bin from pio run"
                : String("Writing failed: ") + Update.errorString();
    firmwareUpdateAbort();
    return false;
  }
  return true;
}

bool firmwareUpdateFinish(String &error) {
  if (!active) {
    error = "No update in progress";
    return false;
  }
  // With the size unknown, whatever arrived is the image; its checksum says
  // whether that is all of it.
  bool ok = Update.end(expected == 0);
  active = false;
  if (!ok) {
    error = String("The file was not accepted: ") + Update.errorString();
    Serial.printf("Firmware: %s\n", error.c_str());
    return false;
  }
  Serial.println("Firmware: new firmware written; restarting to try it");
  return true;
}

void firmwareUpdateAbort() {
  if (active) {
    Update.abort();
    active = false;
    Serial.println("Firmware: update abandoned");
  }
}

bool firmwareUpdateActive() { return active; }

int firmwareUpdatePercent() {
  if (!active) {
    return 0;
  }
  size_t total = expected > 0 ? expected : Update.size();
  return total ? (int)(Update.progress() * 100 / total) : 0;
}

void firmwareUpdateReport(JsonObject out) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  out["slot"] = running->label;
  out["newAndOnProbation"] = onProbation;
  out["updating"] = (bool)active;
}
