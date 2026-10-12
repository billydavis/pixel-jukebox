#include "status.h"

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

void statusSetArtwork(uint32_t postId, const String &owner, const String &gifUrl) {
  Guard guard;
  current.hasArtwork = true;
  current.postId = postId;
  current.owner = owner;
  current.gifUrl = gifUrl;
}

void statusClearArtwork() {
  Guard guard;
  current.hasArtwork = false;
  current.postId = 0;
  current.owner = "";
  current.gifUrl = "";
}

PlayerStatus statusGet() {
  Guard guard;
  return current;
}
