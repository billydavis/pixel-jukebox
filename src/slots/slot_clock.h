#pragma once

#include "clock_rules.h"
#include "slots/slot.h"

// The time and date from the network clock, in one of three styles (Digital,
// Big, Analog) with the options in clock_rules.h, set in Settings > Clock.
// Skipped until the time is known. The night schedule's clock-only nights use
// it too.
class ClockSlot : public Slot {
 public:
  const char *name() const override { return "clock"; }
  bool start() override;
  void shown() override;
  uint32_t update() override;
  bool finished() const override;
  void redraw() override;
  void useSeconds(uint8_t seconds) override { showMs = seconds * 1000UL; }

 private:
  void draw(const struct tm &now);

  uint32_t startedAt = 0;
  uint32_t showMs = 10000;
  int shownSecond = -1; // what is on screen: redrawn when it changes
  int shownMinute = -1;
  uint32_t shownSettings = 0;
  uint32_t shownColor = 0;
};

// The clock's settings, saved on the board. Safe from any task; a change
// shows on the next redraw.
void clockBegin();
clockface::Settings clockSettings();
void clockSetSettings(const clockface::Settings &settings);
