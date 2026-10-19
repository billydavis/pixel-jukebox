#pragma once

#include <stdint.h>

// Which way up the picture goes on the panel (backlog #8): turned in quarter
// turns for however the panel sits in its case, and optionally mirrored (left
// and right swapped), for a live stream or a mirror. Everything is drawn
// upright; display.cpp maps each pixel through this on its way to the panel.
// Kept apart from the firmware so it can be tested.

namespace orientation {

struct Orientation {
  uint8_t quarterTurns = 0; // 0-3, clockwise: 1 is 90 degrees
  bool mirror = false;      // swap left and right, before turning
};

inline bool operator==(const Orientation &a, const Orientation &b) {
  return a.quarterTurns == b.quarterTurns && a.mirror == b.mirror;
}

// Where the upright pixel (x, y) goes on a square panel of `size` pixels.
void map(const Orientation &o, int size, int x, int y, int &panelX, int &panelY);

// The same for a rectangle, which stays a rectangle: x, y, w, h in and out.
void mapRect(const Orientation &o, int size, int &x, int &y, int &w, int &h);

// As stored and sent: 0, 90, 180 or 270. False for anything else.
bool fromDegrees(int value, uint8_t &quarterTurns);
int toDegrees(const Orientation &o);

// Packed into one number for the command queue, and back.
uint32_t pack(const Orientation &o);
Orientation unpack(uint32_t value);

} // namespace orientation
