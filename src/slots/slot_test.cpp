#include "slots/slot_test.h"

#include "display/display.h"
#include "pins.h"

static const uint32_t SHOW_MS = 10UL * 60 * 1000;

bool TestPatternSlot::start() {
  startedAt = millis();
  redraw();
  return true;
}

bool TestPatternSlot::finished() const { return millis() - startedAt >= SHOW_MS; }

void TestPatternSlot::redraw() {
  for (int y = 0; y < PANEL_RES_Y; y++) {
    for (int x = 0; x < PANEL_RES_X; x++) {
      uint8_t r, g, b;
      testpattern::colour(pattern, x, y, r, g, b);
      display->drawPixel(x, y, display->color565(r, g, b));
    }
  }
}
