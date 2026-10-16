#pragma once

#include <ArduinoJson.h>

#include "night_rules.h"

// The night schedule: dims the panel, turns it off, or shows only the clock
// between two times, fading over a few minutes each way. Moving the
// brightness, or pressing Next while it is dark, overrides it for an hour or
// until its next switch, whichever is sooner. With no time known it waits.

struct NightNow {
  bool enabled = false;
  bool timeKnown = false;
  bool night = false;      // between start and end
  bool overridden = false; // someone took over for a while
  bool off = false;        // the panel should be dark and nothing playing
  bool clockOnly = false;  // only the clock should play
  uint8_t brightness = 0;  // the level to show now
  uint32_t secondsToSwitch = 0;
};

void nightBegin();

night::Settings nightSettings();
// Validates and saves; an override is dropped. False with `error` otherwise.
bool nightSetSettings(const night::Settings &settings, String &error);

NightNow nightNow();

// Call often from the display loop: puts the right brightness on the panel.
void nightTick();

// A brightness chosen by hand: the day level by day, an override at night.
void nightSetBrightness(uint8_t level);

// Next pressed (or art asked for) while the schedule has the panel dark or on
// the clock: back to normal for a while.
void nightWake();

void nightReport(JsonObject report);
