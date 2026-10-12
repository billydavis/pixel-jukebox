#pragma once

#include <stdint.h>

// Transitions between two 64x64 images. Each is a pure function from a
// progress value to a whole frame, so they can be tested without a panel:
// at progress 0 the frame is the first image, at 1 it is the second.

namespace transitions {

static const int SIZE = 64;                // images are SIZE x SIZE
static const int PIXELS = SIZE * SIZE;     // RGB565, row by row from the top-left

struct Info {
  const char *name;  // used in settings and the API
  const char *label; // shown to people
};

int count();
const Info &info(int index);

// The index of the transition with this name, or -1.
int find(const char *name);

// Writes the frame at `progress` (0 to 1) into `out`. `seed` picks the
// direction or the random pattern and must stay the same for one transition.
void render(int index, float progress, uint32_t seed, const uint16_t *from, const uint16_t *to,
            uint16_t *out);

} // namespace transitions
