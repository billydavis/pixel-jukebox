#pragma once

#include <Arduino.h>

// One screen in the rotation. The display loop calls start() once, then
// update() repeatedly until finished() or a command ends the slot, then stop().
class Slot {
 public:
  virtual ~Slot() {}

  virtual const char *name() const = 0;

  // Prepares the slot and draws its first screen. Returns false to be skipped
  // this turn (nothing to show yet); stop() is not called in that case.
  virtual bool start() = 0;

  // Draws whatever is due and returns how many milliseconds until the next call.
  virtual uint32_t update() = 0;

  // True once the slot has had its turn. Ignored while the slot is held.
  virtual bool finished() const = 0;

  // Repaints from scratch, after something else has drawn over the slot.
  virtual void redraw() = 0;

  virtual void stop() {}
};
