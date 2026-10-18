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
                      const String &channel) {
  Guard guard;
  current.hasArtwork = true;
  current.postId = postId;
  current.owner = owner;
  current.gifUrl = gifUrl;
  current.fromCache = fromCache;
  current.channel = channel;
  current.uploadId = 0;
  current.uploadName = "";
}

void statusSetUpload(uint32_t uploadId, const String &name) {
  Guard guard;
  current.hasArtwork = true;
  current.postId = 0;
  current.owner = "";
  current.gifUrl = "";
  current.fromCache = true;
  current.channel = "My uploads";
  current.uploadId = uploadId;
  current.uploadName = name;
}

void statusClearArtwork() {
  Guard guard;
  current.hasArtwork = false;
  current.postId = 0;
  current.owner = "";
  current.gifUrl = "";
  current.fromCache = false;
  current.uploadId = 0;
  current.uploadName = "";
}

PlayerStatus statusGet() {
  Guard guard;
  return current;
}
