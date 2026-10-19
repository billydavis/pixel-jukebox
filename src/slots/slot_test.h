#pragma once

#include "slots/slot.h"
#include "test_patterns.h"

// A test pattern for judging the picture (test_patterns.h), asked for from
// Settings. It stays up until Next, "Back to art", or ten minutes pass.
class TestPatternSlot : public Slot {
 public:
  void use(testpattern::Pattern pattern) { this->pattern = pattern; }

  const char *name() const override { return "test"; }
  bool start() override;
  uint32_t update() override { return 250; }
  bool finished() const override;
  void redraw() override;

 private:
  testpattern::Pattern pattern = testpattern::Pattern::Ramps;
  uint32_t startedAt = 0;
};
