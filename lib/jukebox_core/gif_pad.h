#pragma once

#include <stddef.h>
#include <stdint.h>

// AnimatedGIF reads the header in fixed-size chunks and rejects files too small
// to fill them, which rules out most tiny artwork. Files under GIF_SMALL_BYTES
// get a comment block of zeros inserted after the header to make up the size.
// A comment block is ignored by every GIF decoder, so the image is unchanged.

static const size_t GIF_SMALL_BYTES = 255 + 3 * 256 + 32;
static const int GIF_PAD_BLOCKS = 4;
static const size_t GIF_PAD_BYTES = 2 + GIF_PAD_BLOCKS * 256 + 1;

// Pads a small GIF in place and returns its new size. The buffer must have
// GIF_PAD_BYTES of spare room after the file. Anything that is not a small GIF
// is left alone and `size` is returned unchanged.
size_t gifPadSmall(uint8_t *data, size_t size);
