#include "storage/starter_art.h"

#include <algorithm>
#include <vector>

#include "gif_pad.h"
#include "starter_pack.h"

// tools/stage_starter_art.py writes the pack; platformio.ini embeds it.
extern const uint8_t STARTER_PACK_START[] asm("_binary__pio_embed_starter_art_bin_start");
extern const uint8_t STARTER_PACK_END[] asm("_binary__pio_embed_starter_art_bin_end");

static std::vector<starterpack::Piece> pieces;
static std::vector<uint16_t> order; // the shuffled round in progress
static size_t orderAt = 0;
static SemaphoreHandle_t lock; // the display loop and the loader both take pieces

void starterArtBegin() {
  lock = xSemaphoreCreateMutex();
  size_t size = STARTER_PACK_END - STARTER_PACK_START;
  if (!starterpack::parse(STARTER_PACK_START, size, pieces)) {
    Serial.println("Starter art: the built-in pack could not be read");
    return;
  }
  Serial.printf("Starter art: %u pieces, %u KB\n", (unsigned)pieces.size(), (unsigned)(size / 1024));
}

size_t starterArtCount() { return pieces.size(); }

bool starterArtFind(uint16_t id, StarterPiece &piece) {
  if (id < 1 || id > pieces.size()) {
    return false;
  }
  const starterpack::Piece &found = pieces[id - 1];
  piece = {id, found.title.c_str(), found.credit.c_str()};
  return true;
}

bool starterArtBytes(uint16_t id, const uint8_t *&data, size_t &size, bool still) {
  if (id < 1 || id > pieces.size()) {
    return false;
  }
  const starterpack::Piece &piece = pieces[id - 1];
  data = STARTER_PACK_START + (still ? piece.stillOffset : piece.offset);
  size = still ? piece.stillSize : piece.size;
  return true;
}

bool starterArtRead(uint16_t id, uint8_t *&data, size_t &size) {
  const uint8_t *source;
  if (!starterArtBytes(id, source, size)) {
    return false;
  }
  data = (uint8_t *)ps_malloc(size + GIF_PAD_BYTES);
  if (!data) {
    return false;
  }
  memcpy(data, source, size);
  return true;
}

bool starterArtNext(uint16_t &id) {
  if (pieces.empty()) {
    return false;
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  if (orderAt >= order.size()) {
    uint16_t last = order.empty() ? 0 : order.back();
    order.clear();
    for (uint16_t i = 1; i <= pieces.size(); i++) {
      order.push_back(i);
    }
    for (size_t i = order.size() - 1; i > 0; i--) {
      std::swap(order[i], order[esp_random() % (i + 1)]);
    }
    if (order.size() > 1 && order.front() == last) {
      std::swap(order.front(), order.back()); // not the same piece twice running
    }
    orderAt = 0;
  }
  id = order[orderAt++];
  xSemaphoreGive(lock);
  return true;
}

void starterArtReport(JsonObject out) {
  out["count"] = pieces.size();
  out["bytes"] = (uint32_t)(STARTER_PACK_END - STARTER_PACK_START);
}
