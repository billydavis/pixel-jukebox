#include "storage/card.h"

#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <sys/stat.h>

#include "pins.h"

static const char *MOUNT_POINT = "/sdcard";
static const char *TEST_FILE = CARD_ROOT "/.write-test";

static sdmmc_card_t *card = nullptr;
static bool writable = false;
static bool detected = false; // a card answered, even if it could not be mounted
static String problem = "not checked yet";

// Proves the card takes writes, not just reads: write, read back, delete.
static bool writeTest() {
  const char probe[] = "pixel-jukebox";
  FILE *file = fopen(TEST_FILE, "w");
  if (!file) {
    return false;
  }
  bool ok = fwrite(probe, 1, sizeof(probe), file) == sizeof(probe);
  ok = fclose(file) == 0 && ok;

  char back[sizeof(probe)] = {};
  file = fopen(TEST_FILE, "r");
  if (!file) {
    return false;
  }
  ok = fread(back, 1, sizeof(back), file) == sizeof(back) && ok;
  fclose(file);
  remove(TEST_FILE);
  return ok && memcmp(probe, back, sizeof(probe)) == 0;
}

void cardBegin() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20 MHz

  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.clk = (gpio_num_t)SD_CLK_PIN;
  slot.cmd = (gpio_num_t)SD_CMD_PIN;
  slot.d0 = (gpio_num_t)SD_D0_PIN;
  slot.d1 = GPIO_NUM_NC;
  slot.d2 = GPIO_NUM_NC;
  slot.d3 = GPIO_NUM_NC;
  slot.width = 1; // only D0 is wired
  slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  esp_vfs_fat_sdmmc_mount_config_t config = {};
  config.format_if_mount_failed = false; // never: the card may hold someone's files
  config.max_files = 4;
  config.allocation_unit_size = 16 * 1024;

  esp_err_t result = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot, &config, &card);
  if (result != ESP_OK) {
    card = nullptr;
    if (result == ESP_FAIL) {
      detected = true;
      problem = "the card has no FAT file system; format it as FAT32 on a computer";
    } else if (result == ESP_ERR_TIMEOUT || result == ESP_ERR_NOT_FOUND || result == ESP_ERR_INVALID_RESPONSE) {
      problem = "no card";
    } else {
      problem = esp_err_to_name(result);
    }
    Serial.printf("Card: %s (%s)\n", problem.c_str(), esp_err_to_name(result));
    return;
  }

  detected = true;
  mkdir(CARD_ROOT, 0777);
  writable = writeTest();
  problem = writable ? "" : "the card is read-only or failing";

  JsonDocument doc;
  cardReport(doc.to<JsonObject>());
  Serial.printf("Card: %s %s, %llu MB free of %llu MB%s\n", doc["type"].as<const char *>(),
                doc["fileSystem"].as<const char *>(), doc["freeBytes"].as<uint64_t>() >> 20,
                doc["totalBytes"].as<uint64_t>() >> 20, writable ? "" : ", NOT WRITABLE");
}

bool cardReady() { return card && writable; }

void cardReport(JsonObject report) {
  report["present"] = detected;
  report["mounted"] = card != nullptr;
  report["ready"] = cardReady();
  if (!card) {
    report["problem"] = problem;
    return;
  }
  uint64_t capacity = (uint64_t)card->csd.capacity * card->csd.sector_size;
  // Standard SD cards stop at 2 GB; anything larger is SDHC or SDXC.
  report["type"] = card->is_mmc ? "MMC" : capacity > (2ULL << 30) ? "SDHC" : "SD";
  report["capacityBytes"] = capacity;

  FATFS *fs = nullptr;
  DWORD freeClusters = 0;
  if (f_getfree("0:", &freeClusters, &fs) == FR_OK && fs) {
    uint64_t clusterBytes = (uint64_t)fs->csize * 512;
    report["fileSystem"] = fs->fs_type == FS_FAT32 ? "FAT32" : fs->fs_type == FS_FAT16 ? "FAT16" : "FAT12";
    report["totalBytes"] = (uint64_t)(fs->n_fatent - 2) * clusterBytes;
    report["freeBytes"] = (uint64_t)freeClusters * clusterBytes;
  }
  if (!problem.isEmpty()) {
    report["problem"] = problem;
  }
}
