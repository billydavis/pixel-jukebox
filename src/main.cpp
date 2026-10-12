// Pixel Jukebox - shuffles pixel art from Makapix Club on a 64x64 LED matrix.

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#include "audio.h"
#include "display.h"
#include "gif_player.h"
#include "makapix.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and fill in your Wi-Fi details"
#endif

static const uint32_t WIFI_TIMEOUT_MS = 20000;
static const uint32_t RETRY_DELAY_MS = 10000;
static const uint32_t CODE_VALID_MS = 15 * 60 * 1000;
static const uint32_t POLL_INTERVAL_MS = 5000;
static const uint32_t RATE_LIMIT_WAIT_MS = 60000;
static const int MAX_LOAD_ATTEMPTS = 3;
static const uint32_t LOADER_STACK_BYTES = 16384; // TLS handshake plus JSON parsing
static const BaseType_t LOADER_CORE = 0;          // away from the playback loop on core 1

enum class LoadStatus { Ready, Unauthorized, Failed };

struct LoadedArtwork {
  makapix::Artwork artwork;
  uint8_t *data;
  size_t size;
};

// Owned by the loader while it runs, by loop() otherwise.
static std::vector<makapix::Artwork> queue;
static size_t queueIndex = 0;
static LoadedArtwork pending;
static LoadStatus pendingStatus;

static SemaphoreHandle_t loaderDone;
static bool loading = false;

static uint16_t amber() { return display->color565(255, 200, 0); }
static uint16_t red() { return display->color565(255, 0, 0); }
static uint16_t green() { return display->color565(0, 255, 0); }

static bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      return false;
    }
    delay(250);
  }
  return true;
}

static bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  showMessage("WiFi", "joining...", amber());
  if (!connectWifi()) {
    Serial.println("Wi-Fi connection timed out");
    showMessage("WiFi", "failed", red());
    delay(RETRY_DELAY_MS);
    return false;
  }
  Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());
  showMessage("WiFi OK", WiFi.localIP().toString(), green());
  delay(1500);
  return true;
}

// Shows a registration code and waits for the user to enter it on makapix.club.
static void registerDevice() {
  String code;
  if (!makapix::provision(code)) {
    showMessage("Makapix", "unreachable", red());
    delay(RETRY_DELAY_MS);
    return;
  }
  Serial.printf("Registration code: %s\n", code.c_str());
  showRegistrationCode(code);

  uint32_t start = millis();
  while (millis() - start < CODE_VALID_MS) {
    delay(POLL_INTERVAL_MS);
    makapix::LinkStatus status = makapix::pollCredentials();
    if (status == makapix::LinkStatus::Linked) {
      Serial.println("Device linked");
      showMessage("Makapix", "linked!", green());
      delay(1500);
      return;
    }
    if (status == makapix::LinkStatus::RateLimited) {
      delay(RATE_LIMIT_WAIT_MS);
    }
  }
  // Code expired; the next loop() pass provisions a fresh one.
}

// Takes the next artwork from the queue (fetching a new page when it runs out)
// and downloads it, skipping ones that fail to download.
static LoadStatus loadNext(LoadedArtwork &loaded) {
  for (int attempt = 0; attempt < MAX_LOAD_ATTEMPTS; attempt++) {
    if (queueIndex >= queue.size()) {
      queueIndex = 0;
      makapix::QueryStatus status = makapix::nextPage(queue);
      if (status == makapix::QueryStatus::Unauthorized) {
        return LoadStatus::Unauthorized;
      }
      if (status == makapix::QueryStatus::Failed || queue.empty()) {
        return LoadStatus::Failed;
      }
      Serial.printf("Queued %u artworks\n", (unsigned)queue.size());
    }

    const makapix::Artwork &artwork = queue[queueIndex++];
    size_t size = 0;
    uint8_t *data = downloadGif(artwork.gifUrl, size);
    if (data) {
      loaded = {artwork, data, size};
      return LoadStatus::Ready;
    }
  }
  return LoadStatus::Failed;
}

static void loaderTask(void *) {
  pendingStatus = loadNext(pending);
  xSemaphoreGive(loaderDone);
  vTaskDelete(nullptr);
}

// Loads the next artwork in the background so it is ready when the current
// one finishes. The result is collected by taking loaderDone.
static void startLoading() {
  loading = true;
  if (xTaskCreatePinnedToCore(loaderTask, "loader", LOADER_STACK_BYTES, nullptr, 1, nullptr,
                              LOADER_CORE) != pdPASS) {
    pendingStatus = loadNext(pending);
    xSemaphoreGive(loaderDone);
  }
}

// Lists the I2C devices, then plays a rising chime and a steady tone three times.
static void runSpeakerTest() {
  String devices = audioScanI2c();
  Serial.printf("I2C devices: %s\n", devices.c_str());
  showMessage("I2C", devices, amber());
  delay(4000);

  if (!audioBegin()) {
    Serial.println("Audio init failed");
    showMessage("Speaker", "init failed", red());
    delay(5000);
    return;
  }
  Serial.println("Audio init OK, playing tones");
  showMessage("Speaker", "playing", green());
  const float chime[] = {523.25f, 659.25f, 783.99f, 1046.5f}; // C5 E5 G5 C6
  for (int repeat = 0; repeat < 3; repeat++) {
    for (float note : chime) {
      audioTone(note, 250);
    }
    delay(300);
    audioTone(1000, 1000);
    delay(700);
  }
}

void setup() {
  Serial.begin(115200);

  if (!setupDisplay()) {
    Serial.println("Display init failed (DMA buffer allocation)");
    return;
  }
  drawTestPattern();
  delay(1500);
#ifdef COLOUR_CHECK
  runColourCheck();
#endif
#ifdef SPEAKER_TEST
  runSpeakerTest();
#endif

  loaderDone = xSemaphoreCreateBinary();
  makapix::begin();
}

void loop() {
  if (!display) {
    delay(1000);
    return;
  }

  if (!loading) {
    if (!ensureWifi()) {
      return;
    }
    if (!makapix::isRegistered()) {
      registerDevice();
      return;
    }
    startLoading();
  }

  xSemaphoreTake(loaderDone, portMAX_DELAY);
  loading = false;

  if (pendingStatus == LoadStatus::Unauthorized) {
    // Token revoked or device removed on the site: register again.
    Serial.println("Token rejected, registering again");
    makapix::forget();
    queue.clear();
    queueIndex = 0;
    return;
  }
  if (pendingStatus == LoadStatus::Failed) {
    // A dropped Wi-Fi connection is reported by ensureWifi() on the next pass.
    if (WiFi.status() == WL_CONNECTED) {
      showMessage("Makapix", "no artwork", red());
      delay(RETRY_DELAY_MS);
    }
    return;
  }

  LoadedArtwork current = pending;
  startLoading(); // preload the next one while this one plays
  Serial.printf("Playing post %u (%u bytes)\n", (unsigned)current.artwork.postId,
                (unsigned)current.size);
  playGif(current.data, current.size, current.artwork.dwellMs);
  free(current.data);
}
