#pragma once

#include <Arduino.h>
#include <vector>

#include "slot.h"

// The order slots are shown in. Saved on the board and editable through the
// API; every call is safe from any task.

struct RotationEntry {
  String slot;   // a name from rotationAvailable()
  uint8_t turns; // times in a row the slot is shown before moving on
};

struct SlotInfo {
  const char *name;
  const char *label;
  const char *turnMeaning; // what one turn is, for people editing the rotation
};

static const size_t ROTATION_MAX_ENTRIES = 8;
static const uint8_t ROTATION_MAX_TURNS = 20;

// Starts every slot type and loads the saved rotation.
void rotationBegin();

const std::vector<SlotInfo> &rotationAvailable();
std::vector<RotationEntry> rotationGet();

// Validates, saves and restarts the rotation from the top once the slot on
// screen finishes. On failure nothing changes and `error` says why.
bool rotationSet(const std::vector<RotationEntry> &entries, String &error);

// The slot whose turn it is, then move on to the next turn.
Slot *rotationCurrent();
void rotationAdvance();

// Turns in one full pass.
size_t rotationTurns();
