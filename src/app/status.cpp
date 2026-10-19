#include "app/status.h"

static SemaphoreHandle_t lock;
static PlayerStatus current;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

void statusBegin() { lock = xSemaphoreCreateMutex(); }

void statusSetSlot(const char *name) {
  Guard guard;
  current.slot = name;
}

void statusSetHeld(bool held) {
  Guard guard;
  current.held = held;
}

void statusSetScreen(const char *name) {
  Guard guard;
  current.screen = name;
}

void statusSetArtwork(uint32_t postId, const String &owner, const String &gifUrl, bool fromCache,
                      const String &channel, const String &storageKey) {
  Guard guard;
  current.storageKey = storageKey;
  current.hasArtwork = true;
  current.postId = postId;
  current.owner = owner;
  current.gifUrl = gifUrl;
  current.fromCache = fromCache;
  current.channel = channel;
  current.uploadId = 0;
  current.uploadName = "";
  current.starterId = 0;
  current.starterTitle = "";
  current.starterCredit = "";
}

void statusSetUpload(uint32_t uploadId, const String &name) {
  Guard guard;
  current.hasArtwork = true;
  current.postId = 0;
  current.owner = "";
  current.gifUrl = "";
  current.storageKey = "";
  current.fromCache = true;
  current.channel = "My uploads";
  current.uploadId = uploadId;
  current.uploadName = name;
  current.starterId = 0;
  current.starterTitle = "";
  current.starterCredit = "";
}

void statusSetStarter(uint16_t id, const String &title, const String &credit, bool fallback) {
  Guard guard;
  current.hasArtwork = true;
  current.postId = 0;
  current.owner = "";
  current.gifUrl = "";
  current.storageKey = "";
  current.fromCache = true;
  current.channel = fallback ? "" : "Starter art";
  current.uploadId = 0;
  current.uploadName = "";
  current.starterId = id;
  current.starterTitle = title;
  current.starterCredit = credit;
}

void statusClearArtwork() {
  Guard guard;
  current.hasArtwork = false;
  current.postId = 0;
  current.owner = "";
  current.gifUrl = "";
  current.storageKey = "";
  current.fromCache = false;
  current.uploadId = 0;
  current.uploadName = "";
  current.starterId = 0;
  current.starterTitle = "";
  current.starterCredit = "";
}

PlayerStatus statusGet() {
  Guard guard;
  return current;
}
