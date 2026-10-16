#pragma once

#include <stdint.h>
#include <string>

// The night schedule's arithmetic, kept apart from the clock and the panel so
// it can be tested. Times are local, in minutes or seconds since midnight.
//
// Night runs from `start` to `end`, wrapping past midnight when `end` is the
// earlier time (22:00 to 07:00). When it starts the brightness fades from the
// day level to the night level over `fadeMinutes`, and back again when it ends.

namespace night {

enum class Action : uint8_t {
  Dim,   // art keeps playing at the night brightness
  Off,   // the panel fades to black, then nothing plays
  Clock, // only the clock, at the night brightness
};

struct Settings {
  bool enabled = false;
  uint16_t start = 22 * 60; // minutes since midnight
  uint16_t end = 7 * 60;
  Action action = Action::Dim;
  uint8_t brightness = 8; // the night level, for Dim and Clock
  uint8_t fadeMinutes = 10;
};

static const uint8_t MAX_FADE_MINUTES = 60;
static const uint32_t SECONDS_PER_DAY = 86400;

// "22:00" to 1320 and back. False for anything but HH:MM from 00:00 to 23:59.
bool parseTime(const std::string &text, uint16_t &minutes);
std::string formatTime(uint16_t minutes);

const char *actionName(Action action);
bool actionFromName(const std::string &name, Action &action);

// False, with `error`, when start equals end or a number is out of range.
bool valid(const Settings &settings, std::string &error);

bool isNight(const Settings &settings, uint32_t secondOfDay);

// How far into night the brightness is: 0 by day, 1 by night, in between
// during the fades. 0 when the schedule is off.
float depth(const Settings &settings, uint32_t secondOfDay);

// The level `depth` of the way from the day level to the night level.
uint8_t blend(uint8_t day, uint8_t night, float depth);

// Seconds from `secondOfDay` to the next start or end of night.
uint32_t secondsToNextSwitch(const Settings &settings, uint32_t secondOfDay);

} // namespace night
