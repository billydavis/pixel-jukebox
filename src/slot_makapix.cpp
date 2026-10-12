#include "slot_makapix.h"

#include <WiFi.h>
#include <vector>

#include "display.h"
#include "gif_player.h"
#include "status.h"
#include "telemetry.h"

static const uint32_t CODE_VALID_MS = 15 * 60 * 1000;
static const uint32_t CODE_SHOW_MS = 45000; // per turn, so other slots still get shown
static const uint32_t POLL_INTERVAL_MS = 5000;
static const uint32_t RATE_LIMIT_WAIT_MS = 60000;
static const uint32_t LINKED_SHOW_MS = 1500;
static const uint32_t LOAD_WAIT_MS = 15000;
static const uint32_t IDLE_UPDATE_MS = 100;
static const int MAX_LOAD_ATTEMPTS = 5;
static const uint32_t LOADER_STACK_BYTES = 16384; // TLS handshake plus JSON parsing
static const BaseType_t LOADER_CORE = 0;          // away from the display loop on core 1

enum class LoadStatus { Ready, Unauthorized, Failed };

struct LoadedArtwork {
  makapix::Artwork artwork;
  uint8_t *data;
  size_t size;
};

// Owned by the loader while it runs, by the slot otherwise.
static std::vector<makapix::Artwork> queue;
static size_t queueIndex = 0;
static LoadedArtwork pending;
static LoadStatus pendingStatus;

static SemaphoreHandle_t loaderDone;
static bool loading = false;

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
    if (!data) {
      continue;
    }
    // The vault sometimes answers with something that is not a GIF.
    if (size < 6 || memcmp(data, "GIF8", 4) != 0) {
      Serial.printf("Not a GIF, skipping: %s\n", artwork.gifUrl.c_str());
      free(data);
      continue;
    }
    telemetryNoteImage(size);
    loaded = {artwork, data, size};
    return LoadStatus::Ready;
  }
  return LoadStatus::Failed;
}

static void loaderTask(void *) {
  pendingStatus = loadNext(pending);
  telemetryNoteTaskExit("loader", LOADER_STACK_BYTES);
  xSemaphoreGive(loaderDone);
  vTaskDelete(nullptr);
}

// Loads the next artwork in the background so it is ready for the next turn.
// The result is collected by taking loaderDone.
static void startLoading() {
  loading = true;
  if (xTaskCreatePinnedToCore(loaderTask, "loader", LOADER_STACK_BYTES, nullptr, 1, nullptr,
                              LOADER_CORE) != pdPASS) {
    pendingStatus = loadNext(pending);
    xSemaphoreGive(loaderDone);
  }
}

void MakapixSlot::begin() {
  loaderDone = xSemaphoreCreateBinary();
  makapix::begin();
}

bool MakapixSlot::start() {
  mode = Mode::Idle;
  if (WiFi.status() != WL_CONNECTED && !loading) {
    return false;
  }
  return makapix::isRegistered() ? startPlaying() : startRegistration();
}

// Shows a registration code for the user to enter on makapix.club. The same
// code is reused on later turns until it expires.
bool MakapixSlot::startRegistration() {
  if (code.isEmpty() || millis() - codeIssuedAt >= CODE_VALID_MS) {
    if (!makapix::provision(code)) {
      code = "";
      return false;
    }
    codeIssuedAt = millis();
    Serial.printf("Registration code: %s\n", code.c_str());
  }

  mode = Mode::Registering;
  startedAt = millis();
  lastPollAt = millis();
  pollIntervalMs = POLL_INTERVAL_MS;
  linked = false;
  showRegistrationCode(code);
  return true;
}

bool MakapixSlot::startPlaying() {
  if (!loading) {
    startLoading();
  }
  if (xSemaphoreTake(loaderDone, pdMS_TO_TICKS(LOAD_WAIT_MS)) != pdTRUE) {
    return false; // still downloading; it will be ready on a later turn
  }
  loading = false;

  if (pendingStatus == LoadStatus::Unauthorized) {
    // Token revoked or device removed on the site: register again.
    Serial.println("Token rejected, registering again");
    makapix::forget();
    queue.clear();
    queueIndex = 0;
    return startRegistration();
  }
  if (pendingStatus == LoadStatus::Failed) {
    return false;
  }

  LoadedArtwork current = pending;
  startLoading(); // preload the next one while this one plays
  Serial.printf("Playing post %u (%u bytes)\n", (unsigned)current.artwork.postId,
                (unsigned)current.size);
  if (!gifOpen(current.data, current.size)) {
    free(current.data);
    return false;
  }

  // The panel fetches over plain HTTP; other clients get the HTTPS address.
  String shareUrl = current.artwork.gifUrl;
  if (shareUrl.startsWith("http://")) {
    shareUrl = "https://" + shareUrl.substring(7);
  }
  statusSetArtwork(current.artwork.postId, current.artwork.ownerHandle, shareUrl);
  mode = Mode::Playing;
  startedAt = millis();
  data = current.data;
  dwellMs = current.artwork.dwellMs;
  framesThisLoop = 0;
  loopEnded = false;
  still = false;
  failed = false;
  return true;
}

uint32_t MakapixSlot::update() {
  switch (mode) {
    case Mode::Registering:
      return updateRegistration();
    case Mode::Playing:
      return updatePlaying();
    default:
      return IDLE_UPDATE_MS;
  }
}

uint32_t MakapixSlot::updateRegistration() {
  if (linked || millis() - lastPollAt < pollIntervalMs) {
    return IDLE_UPDATE_MS;
  }
  lastPollAt = millis();

  makapix::LinkStatus status = makapix::pollCredentials();
  pollIntervalMs =
      status == makapix::LinkStatus::RateLimited ? RATE_LIMIT_WAIT_MS : POLL_INTERVAL_MS;
  if (status == makapix::LinkStatus::Linked) {
    Serial.println("Device linked");
    code = "";
    linked = true;
    linkedAt = millis();
    showMessage("Makapix", "linked!", display->color565(0, 255, 0));
  }
  return IDLE_UPDATE_MS;
}

uint32_t MakapixSlot::updatePlaying() {
  if (failed || (still && framesThisLoop > 0)) {
    return IDLE_UPDATE_MS; // nothing left to draw
  }

  if (loopEnded) {
    gifRestart();
    framesThisLoop = 0;
    loopEnded = false;
  }

  int frameMs = 0;
  int more = gifPlayFrame(frameMs);
  if (more < 0) {
    failed = true;
    return 0;
  }
  framesThisLoop++;

  if (more == 0) {
    if (framesThisLoop == 1) {
      still = true;
      return IDLE_UPDATE_MS;
    }
    loopEnded = true; // rewound on the next call, after this frame has had its time
  }
  return frameMs;
}

bool MakapixSlot::finished() const {
  switch (mode) {
    case Mode::Registering:
      return linked ? millis() - linkedAt >= LINKED_SHOW_MS : millis() - startedAt >= CODE_SHOW_MS;
    case Mode::Playing:
      // Animations end on a loop boundary, never part-way through.
      return failed || (millis() - startedAt >= dwellMs && (still || loopEnded));
    default:
      return true;
  }
}

void MakapixSlot::redraw() {
  if (mode == Mode::Registering && !linked) {
    showRegistrationCode(code);
  } else if (mode == Mode::Playing && !failed) {
    display->clearScreen();
    gifRestart();
    framesThisLoop = 0;
    loopEnded = false;
    still = false;
  }
}

void MakapixSlot::stop() {
  if (mode == Mode::Playing) {
    gifClose();
    free(data);
    data = nullptr;
    statusClearArtwork();
  }
  mode = Mode::Idle;
}
