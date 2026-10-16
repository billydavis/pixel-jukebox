#pragma once

#include <ArduinoJson.h>

// The board's PCF85063 clock chip, which keeps time while the board is
// powered, through restarts and crashes, and through power cuts too if a
// backup battery is fitted. At start-up it sets the system clock, so the clock
// slot and view reports have the time before Wi-Fi is up; whenever the
// network time arrives, the chip is set from it.

// Call once at start-up, before Wi-Fi.
void clockChipBegin();

// Call regularly from the display loop: sets the chip after a network sync.
void clockChipMaintain();

void clockReport(JsonObject report);
