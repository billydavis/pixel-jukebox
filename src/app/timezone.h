#pragma once

#include <Arduino.h>

// The clock's time zone (timezones.h), chosen in Settings > Clock and saved on
// the board; US Central until one is. Applied to the C library's local time,
// which the clock, the night schedule and the clock chip all use.

// Call first thing at start-up: loads the zone and applies it.
void timezoneBegin();

// The zone in use (its index in timezones.h), or the one asked for and about
// to be applied. Safe from any task.
int timezoneIndex();
const char *timezonePosix();

// Saves `index` as the zone; timezoneApply() then puts it in force. Safe from
// any task; false for an index that is not a zone.
bool timezoneSave(int index);
// Display loop only: puts the saved zone in force.
void timezoneApply();
