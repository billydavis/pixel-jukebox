#include "orientation.h"

namespace orientation {

void map(const Orientation &o, int size, int x, int y, int &panelX, int &panelY) {
  int last = size - 1;
  if (o.mirror) {
    x = last - x;
  }
  switch (o.quarterTurns & 3) {
    case 1: // 90 degrees clockwise: the top row becomes the right-hand column
      panelX = last - y;
      panelY = x;
      break;
    case 2:
      panelX = last - x;
      panelY = last - y;
      break;
    case 3:
      panelX = y;
      panelY = last - x;
      break;
    default:
      panelX = x;
      panelY = y;
      break;
  }
}

void mapRect(const Orientation &o, int size, int &x, int &y, int &w, int &h) {
  int ax, ay, bx, by;
  map(o, size, x, y, ax, ay);
  map(o, size, x + w - 1, y + h - 1, bx, by);
  x = ax < bx ? ax : bx;
  y = ay < by ? ay : by;
  w = (ax < bx ? bx - ax : ax - bx) + 1;
  h = (ay < by ? by - ay : ay - by) + 1;
}

bool fromDegrees(int value, uint8_t &quarterTurns) {
  if (value < 0 || value > 270 || value % 90 != 0) {
    return false;
  }
  quarterTurns = (uint8_t)(value / 90);
  return true;
}

int toDegrees(const Orientation &o) { return (o.quarterTurns & 3) * 90; }

uint32_t pack(const Orientation &o) { return (uint32_t)(o.quarterTurns & 3) | (o.mirror ? 4u : 0u); }

Orientation unpack(uint32_t value) {
  Orientation o;
  o.quarterTurns = (uint8_t)(value & 3);
  o.mirror = (value & 4) != 0;
  return o;
}

} // namespace orientation
