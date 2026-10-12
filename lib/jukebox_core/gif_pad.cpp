#include "gif_pad.h"

#include <string.h>

size_t gifPadSmall(uint8_t *data, size_t size) {
  if (size >= GIF_SMALL_BYTES || size < 13 || memcmp(data, "GIF8", 4) != 0) {
    return size;
  }
  // The header is 13 bytes, followed by the global colour table if there is one.
  uint8_t flags = data[10];
  size_t headerBytes = 13 + ((flags & 0x80) ? 3 * (1 << ((flags & 7) + 1)) : 0);
  if (headerBytes >= size) {
    return size;
  }

  uint8_t *pad = data + headerBytes;
  memmove(pad + GIF_PAD_BYTES, pad, size - headerBytes);
  memset(pad, 0, GIF_PAD_BYTES); // the final zero ends the comment
  pad[0] = 0x21;                 // extension
  pad[1] = 0xFE;                 // comment
  for (int block = 0; block < GIF_PAD_BLOCKS; block++) {
    pad[2 + block * 256] = 255; // sub-block length
  }
  return size + GIF_PAD_BYTES;
}
