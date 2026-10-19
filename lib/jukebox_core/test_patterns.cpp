#include "test_patterns.h"

#include <string.h>

namespace testpattern {

static const int SIZE = 64;

const char *name(Pattern pattern) {
  switch (pattern) {
    case Pattern::Darks:
      return "darks";
    case Pattern::Bars:
      return "bars";
    case Pattern::White:
      return "white";
    default:
      return "ramps";
  }
}

const char *label(Pattern pattern) {
  switch (pattern) {
    case Pattern::Darks:
      return "Dark steps";
    case Pattern::Bars:
      return "Colour bars";
    case Pattern::White:
      return "White";
    default:
      return "Ramps";
  }
}

bool fromName(const char *text, Pattern &pattern) {
  for (int i = 0; i < COUNT; i++) {
    if (strcmp(text, name((Pattern)i)) == 0) {
      pattern = (Pattern)i;
      return true;
    }
  }
  return false;
}

// A 5-bit level (0-31) as 8 bits, exact in RGB565 for all three channels.
static uint8_t level(int five) { return (uint8_t)(five << 3); }

void colour(Pattern pattern, int x, int y, uint8_t &r, uint8_t &g, uint8_t &b) {
  r = g = b = 0;
  switch (pattern) {
    case Pattern::Ramps: {
      // 32 steps across, two pixels each; four bands of 16 rows.
      uint8_t v = level(x / 2);
      int band = y / 16;
      r = (band == 0 || band == 1) ? v : 0;
      g = (band == 0 || band == 2) ? v : 0;
      b = (band == 0 || band == 3) ? v : 0;
      return;
    }
    case Pattern::Darks: {
      // Top half levels 0-15, bottom half 16-31, four pixels a band, with a
      // dark row between the halves.
      if (y == SIZE / 2 - 1 || y == SIZE / 2) {
        return;
      }
      int step = x / 4 + (y < SIZE / 2 ? 0 : 16);
      r = g = b = level(step);
      return;
    }
    case Pattern::Bars: {
      // White, yellow, cyan, green, magenta, red, blue, black, eight pixels
      // each. Each entry's bits: 4 red, 2 green, 1 blue.
      static const uint8_t BARS[8] = {7, 6, 3, 2, 5, 4, 1, 0};
      uint8_t bits = BARS[x / 8];
      r = (bits & 4) ? 255 : 0;
      g = (bits & 2) ? 255 : 0;
      b = (bits & 1) ? 255 : 0;
      return;
    }
    case Pattern::White:
      r = g = b = 255;
      return;
  }
}

} // namespace testpattern
