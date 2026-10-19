#pragma once

#include <stdint.h>

// Test patterns for judging the picture (Settings, Test pattern): what colour
// each pixel of the 64 x 64 panel is. Colours are 8-bit, chosen to be exact
// in RGB565, which is what artwork reaches the display driver as.

namespace testpattern {

enum class Pattern : uint8_t {
  Ramps, // grey, red, green and blue, dark to light, a band each
  Darks, // the 32 grey levels artwork can have, 16 bands to a half: any band
         // darker than the one before it is the timing fault docs/colour.md describes
  Bars,  // white, yellow, cyan, green, magenta, red, blue, black
  White, // all white, for white balance
};

static const int COUNT = 4;

const char *name(Pattern pattern);  // "ramps", "darks", "bars", "white"
const char *label(Pattern pattern); // "Ramps", "Dark steps", "Colour bars", "White"
bool fromName(const char *name, Pattern &pattern);

void colour(Pattern pattern, int x, int y, uint8_t &r, uint8_t &g, uint8_t &b);

} // namespace testpattern
