#pragma once

#include <Arduino.h>
#include <vector>

#include "slots/slot.h"

class MakapixSlot;

// The order slots are shown in. Saved on the board and editable through the
// API; every call is safe from any task.

struct RotationEntry {
  String slot;    // a name from rotationAvailable()
  uint8_t turns;  // times in a row the slot is shown before moving on
  String channel; // for "makapix" slots, which art (channels.h); empty for the others
  bool enabled;   // false: paused, kept in its place but skipped
};

struct SlotInfo {
  const char *name;
  const char *label;
  const char *turnMeaning; // what one turn is, for people editing the rotation
};

static const size_t ROTATION_MAX_ENTRIES = 12;
static const uint8_t ROTATION_MAX_TURNS = 20;

// Starts every slot type and loads the saved rotation.
void rotationBegin();

const std::vector<SlotInfo> &rotationAvailable();
std::vector<RotationEntry> rotationGet();

// Validates, saves and restarts the rotation from the top once the slot on
// screen finishes. On failure nothing changes and `error` says why. Channels
// are stored in their canonical form ("artist:@Fab" becomes "artist:Fab").
bool rotationSet(const std::vector<RotationEntry> &entries, String &error);

// One channel played by every artwork slot for now, in place of their own:
// set from makapix.club (play_channel) or the API. Not saved, so a restart
// goes back to the rotation's channels; so does changing the rotation.
// `from` says who asked ("makapix.club", "api"). An empty spec clears it.
// False with `error` for a spec that is not a channel.
bool rotationSetChannelOverride(const String &spec, const char *from, String &error);
// The spec in force, or empty; `from` says who set it.
String rotationChannelOverride(String *from = nullptr);

// The slot whose turn it is, then move on to the next turn.
Slot *rotationCurrent();
void rotationAdvance();

// Turns in one full pass, counting only slots that are on.
size_t rotationTurns();

// The artwork slot, for showing one artwork outside the rotation.
MakapixSlot *rotationArtworkSlot();

// The clock slot, for the night schedule's clock-only nights.
Slot *rotationClockSlot();
