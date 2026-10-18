#pragma once

#include <stddef.h>
#include <stdint.h>

// What is on the panel, decided in one place. Several things can take over
// from the rotation: the night schedule (dark, or only the clock), sound mode,
// and an artwork someone asked for (from the gallery, or from makapix.club).
// These rules say which one wins and what Next and Hold mean for each, so the
// display loop does not grow an "except in ..." for every new feature.

namespace screen {

enum class Source : uint8_t {
  Dark,       // the night schedule has the panel off
  Request,    // one artwork asked for, shown for one turn
  Sound,      // a visualizer, until the mode changes
  NightClock, // the clock in place of the rotation, for the night
  Rotation,   // the slots in turn
};

struct Inputs {
  bool nightOff = false;
  bool nightClock = false;
  bool sound = false;
  bool requestPending = false;
};

// A request comes first: it wakes a dark panel (the caller does the waking)
// and plays one turn, after which whatever was on before carries on. Then the
// night schedule's dark, then sound mode, then the night clock, then the
// rotation.
Source choose(const Inputs &inputs);

// For /status: "dark", "request", "sound", "nightClock", "rotation".
const char *name(Source source);

// Whether what is running should give way, because what would be chosen now
// (with no new request) is something else. A request is never cut short.
bool shouldGiveWay(Source running, const Inputs &now);

// Only the rotation moves on to its next slot after a turn.
bool advancesRotation(Source source);

// Hold keeps an artwork or the clock on screen. A visualizer already stays
// put, and a dark panel has nothing to hold.
bool holdAllowed(Source source);

// Next steps through the visualizers in sound mode; everywhere else it ends
// the turn.
bool nextChangesVisualizer(Source source);

// A blend between slots, except into a visualizer (it starts at once) and from
// the night clock into itself.
bool transitionAllowed(Source source, Source previous);

// The artworks shown, most recent last, so Back can go to the one before. An
// artwork shown by Back is not added again, so pressing Back repeatedly walks
// further back.
class History {
public:
  static const size_t MAX = 16;

  void shown(uint32_t postId);
  // The artwork before the current one, or 0 when there is none. The current
  // one is dropped, so the answer becomes the current one.
  uint32_t back();
  size_t size() const { return count_; }

private:
  uint32_t ids_[MAX] = {};
  size_t count_ = 0;
};

} // namespace screen
