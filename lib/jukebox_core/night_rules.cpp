#include "night_rules.h"

#include <stdio.h>

namespace night {

bool parseTime(const std::string &text, uint16_t &minutes) {
  if (text.size() != 5 || text[2] != ':') {
    return false;
  }
  for (int i : {0, 1, 3, 4}) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
  }
  int hours = (text[0] - '0') * 10 + (text[1] - '0');
  int mins = (text[3] - '0') * 10 + (text[4] - '0');
  if (hours > 23 || mins > 59) {
    return false;
  }
  minutes = (uint16_t)(hours * 60 + mins);
  return true;
}

std::string formatTime(uint16_t minutes) {
  char text[6];
  snprintf(text, sizeof(text), "%02u:%02u", (unsigned)(minutes / 60 % 24), (unsigned)(minutes % 60));
  return text;
}

const char *actionName(Action action) {
  switch (action) {
    case Action::Off:
      return "off";
    case Action::Clock:
      return "clock";
    default:
      return "dim";
  }
}

bool actionFromName(const std::string &name, Action &action) {
  if (name == "dim") {
    action = Action::Dim;
  } else if (name == "off") {
    action = Action::Off;
  } else if (name == "clock") {
    action = Action::Clock;
  } else {
    return false;
  }
  return true;
}

bool valid(const Settings &settings, std::string &error) {
  if (settings.start >= 24 * 60 || settings.end >= 24 * 60) {
    error = "times must be from 00:00 to 23:59";
    return false;
  }
  if (settings.start == settings.end) {
    error = "night must start and end at different times";
    return false;
  }
  if (settings.fadeMinutes > MAX_FADE_MINUTES) {
    error = "the fade can be at most 60 minutes";
    return false;
  }
  return true;
}

// Seconds from `from` forward to `to`, round the clock.
static uint32_t ahead(uint32_t from, uint32_t to) { return (to + SECONDS_PER_DAY - from) % SECONDS_PER_DAY; }

bool isNight(const Settings &settings, uint32_t secondOfDay) {
  if (!settings.enabled || settings.start == settings.end) {
    return false;
  }
  uint32_t start = settings.start * 60u, end = settings.end * 60u;
  // Night is the stretch from start forward to end.
  return ahead(start, secondOfDay % SECONDS_PER_DAY) < ahead(start, end);
}

float depth(const Settings &settings, uint32_t secondOfDay) {
  if (!settings.enabled || settings.start == settings.end) {
    return 0;
  }
  secondOfDay %= SECONDS_PER_DAY;
  uint32_t fade = settings.fadeMinutes * 60u;
  if (isNight(settings, secondOfDay)) {
    uint32_t since = ahead(settings.start * 60u, secondOfDay);
    return fade == 0 || since >= fade ? 1.0f : (float)since / (float)fade;
  }
  uint32_t since = ahead(settings.end * 60u, secondOfDay);
  return fade == 0 || since >= fade ? 0.0f : 1.0f - (float)since / (float)fade;
}

uint8_t blend(uint8_t day, uint8_t night, float depth) {
  if (depth <= 0) {
    return day;
  }
  if (depth >= 1) {
    return night;
  }
  float level = day + (night - day) * depth;
  return (uint8_t)(level + 0.5f);
}

uint32_t secondsToNextSwitch(const Settings &settings, uint32_t secondOfDay) {
  secondOfDay %= SECONDS_PER_DAY;
  uint32_t toStart = ahead(secondOfDay, settings.start * 60u);
  uint32_t toEnd = ahead(secondOfDay, settings.end * 60u);
  if (toStart == 0) toStart = SECONDS_PER_DAY;
  if (toEnd == 0) toEnd = SECONDS_PER_DAY;
  return toStart < toEnd ? toStart : toEnd;
}

} // namespace night
