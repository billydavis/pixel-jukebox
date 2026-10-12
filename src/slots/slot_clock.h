#pragma once

#include "slots/slot.h"

// Date and time from the network clock. Skipped until the time is known.
class ClockSlot : public Slot {
 public:
  const char *name() const override { return "clock"; }
  bool start() override;
  void shown() override;
  uint32_t update() override;
  bool finished() const override;
  void redraw() override;

 private:
  void draw(const struct tm &now);

  uint32_t startedAt = 0;
  int shownMinute = -1;
};
