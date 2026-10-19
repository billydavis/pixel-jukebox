#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

// The starter art built into the firmware (backlog #28) is one file, packed by
// tools/stage_starter_art.py from assets/starter/:
//
//   "PJSA"  version (1 byte, 2)  count (1 byte)
//   per piece: offset (4 bytes, little-endian, from the start of the file)
//              size (4 bytes)  still offset (4 bytes)  still size (4 bytes)
//              title length (1 byte)  title  credit length (1 byte)  credit
//   then the GIFs
//
// The still is one frame of the piece, a small GIF for thumbnails.
//
// Kept apart from the firmware so it can be tested.

namespace starterpack {

struct Piece {
  uint32_t offset;
  uint32_t size;
  uint32_t stillOffset;
  uint32_t stillSize;
  std::string title;
  std::string credit; // who made it, and the pack it came from
};

// The pieces in `data`, or false when it is not a pack this firmware reads,
// or any piece runs past the end or is not a GIF.
bool parse(const uint8_t *data, size_t size, std::vector<Piece> &pieces);

} // namespace starterpack
