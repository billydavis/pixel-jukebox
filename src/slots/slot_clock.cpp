#include "slots/slot_clock.h"

#include <time.h>

#include "display/display.h"
#include "pins.h"

static const uint32_t SHOW_MS = 10000;
static const uint32_t UPDATE_MS = 200;

bool ClockSlot::start() {
  struct tm now;
  if (!getLocalTime(&now, 0)) {
    return false; // not synchronised yet
  }
  startedAt = millis();
  draw(now);
  return true;
}

void ClockSlot::shown() { startedAt = millis(); }

uint32_t ClockSlot::update() {
  struct tm now;
  if (getLocalTime(&now, 0) && now.tm_min != shownMinute) {
    draw(now);
  }
  return UPDATE_MS;
}

bool ClockSlot::finished() const { return millis() - startedAt >= SHOW_MS; }

void ClockSlot::redraw() { shownMinute = -1; }

static void printCentred(const char *text, int y, uint8_t size, uint16_t colour) {
  int width = strlen(text) * 6 * size - size; // 5-pixel glyphs with a 1-pixel gap
  display->setTextSize(size);
  display->setTextColor(colour);
  display->setCursor((PANEL_RES_X - width) / 2, y);
  display->print(text);
}

void ClockSlot::draw(const struct tm &now) {
  shownMinute = now.tm_min;

  char monthDay[8];
  char date[16];
  char clock[8];
  strftime(monthDay, sizeof(monthDay), "%a %b", &now);
  snprintf(date, sizeof(date), "%s %d", monthDay, now.tm_mday);
  int hour = now.tm_hour % 12;
  snprintf(clock, sizeof(clock), "%d:%02d", hour == 0 ? 12 : hour, now.tm_min);

  display->clearScreen();
  display->setTextWrap(false);
  printCentred(date, 8, 1, display->color565(255, 200, 0));
  printCentred(clock, 24, 2, display->color565(255, 255, 255));
  printCentred(now.tm_hour < 12 ? "AM" : "PM", 46, 1, display->color565(120, 120, 120));
}
