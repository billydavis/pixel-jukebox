#pragma once

#include <stddef.h>

// The time zones the clock can be set to, chosen in Settings > Clock: the
// common ones, with their daylight-saving rules as POSIX TZ strings, from
// https://github.com/nayarsystems/posix_tz_db (zones.csv). Kept apart from the
// firmware so it can be tested.

namespace timezones {

struct Zone {
  const char *name;  // IANA name, as stored and sent: "America/Chicago"
  const char *label; // for people: "US Central (Chicago)"
  const char *posix; // for the C library: "CST6CDT,M3.2.0,M11.1.0"
};

size_t count();
const Zone &at(size_t index);
// The index of the zone named `name`, or -1.
int find(const char *name);
// The zone used until one is chosen: US Central, as the firmware always was.
int defaultIndex();

} // namespace timezones
