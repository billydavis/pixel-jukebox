#include "starter_pack.h"

#include <string.h>

namespace starterpack {

static const uint8_t VERSION = 2;

namespace {
struct Reader {
  const uint8_t *data;
  size_t size;
  size_t at;

  bool u8(uint8_t &out) {
    if (at + 1 > size) {
      return false;
    }
    out = data[at++];
    return true;
  }
  bool u32(uint32_t &out) {
    if (at + 4 > size) {
      return false;
    }
    out = (uint32_t)data[at] | (uint32_t)data[at + 1] << 8 | (uint32_t)data[at + 2] << 16 |
          (uint32_t)data[at + 3] << 24;
    at += 4;
    return true;
  }
  bool text(std::string &out) {
    uint8_t length;
    if (!u8(length) || at + length > size) {
      return false;
    }
    out.assign((const char *)data + at, length);
    at += length;
    return true;
  }
};
} // namespace

static bool isGif(const uint8_t *data, size_t size, uint32_t offset, uint32_t length) {
  return length >= 6 && offset <= size && length <= size - offset && memcmp(data + offset, "GIF8", 4) == 0;
}

bool parse(const uint8_t *data, size_t size, std::vector<Piece> &pieces) {
  pieces.clear();
  if (!data || size < 6 || memcmp(data, "PJSA", 4) != 0 || data[4] != VERSION) {
    return false;
  }
  Reader reader{data, size, 5};
  uint8_t count;
  reader.u8(count);
  std::vector<Piece> found;
  for (uint8_t i = 0; i < count; i++) {
    Piece piece;
    if (!reader.u32(piece.offset) || !reader.u32(piece.size) || !reader.u32(piece.stillOffset) ||
        !reader.u32(piece.stillSize) || !reader.text(piece.title) || !reader.text(piece.credit)) {
      return false;
    }
    if (!isGif(data, size, piece.offset, piece.size) || !isGif(data, size, piece.stillOffset, piece.stillSize)) {
      return false;
    }
    found.push_back(piece);
  }
  for (const Piece &piece : found) {
    if (piece.offset < reader.at || piece.stillOffset < reader.at) {
      return false; // a GIF over the index
    }
  }
  pieces = found;
  return true;
}

} // namespace starterpack
