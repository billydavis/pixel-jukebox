// Pixel Jukebox - shuffles pixel art from Makapix Club on a 64x64 LED matrix.

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

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

static std::vector<makapix::Artwork> queue;
static size_t queueIndex = 0;

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

static void refillQueue() {
  queueIndex = 0;
  makapix::QueryStatus status = makapix::nextPage(queue);
  if (status == makapix::QueryStatus::Unauthorized) {
    // Token revoked or device removed on the site: register again.
    Serial.println("Token rejected, registering again");
    makapix::forget();
    return;
  }
  if (status == makapix::QueryStatus::Failed || queue.empty()) {
    showMessage("Makapix", "no artwork", red());
    delay(RETRY_DELAY_MS);
    return;
  }
  Serial.printf("Queued %u artworks\n", (unsigned)queue.size());
}

void setup() {
  Serial.begin(115200);

  if (!setupDisplay()) {
    Serial.println("Display init failed (DMA buffer allocation)");
    return;
  }
  drawTestPattern();
  delay(1500);

  makapix::begin();
}

void loop() {
  if (!display) {
    delay(1000);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    showMessage("WiFi", "joining...", amber());
    if (!connectWifi()) {
      Serial.println("Wi-Fi connection timed out");
      showMessage("WiFi", "failed", red());
      delay(RETRY_DELAY_MS);
      return;
    }
    Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());
    showMessage("WiFi OK", WiFi.localIP().toString(), green());
    delay(1500);
  }

  if (!makapix::isRegistered()) {
    registerDevice();
    return;
  }

  if (queueIndex >= queue.size()) {
    refillQueue();
    return;
  }

  // The previous artwork stays on the panel while the next one downloads.
  const makapix::Artwork &artwork = queue[queueIndex++];
  size_t size = 0;
  uint8_t *data = downloadGif(artwork.gifUrl, size);
  if (!data) {
    delay(1000);
    return;
  }
  Serial.printf("Playing post %u (%u bytes)\n", (unsigned)artwork.postId, (unsigned)size);
  playGif(data, size, artwork.dwellMs);
  free(data);
}
