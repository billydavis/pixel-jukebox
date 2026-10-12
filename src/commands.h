#pragma once

#include <Arduino.h>

// Commands for the display loop. Anything may send them (the BOOT button now,
// an HTTP API later); only the display loop receives them.
enum class Command : uint8_t {
  Next,       // skip to the next slot
  ToggleHold, // stay on the current slot, or release it
};

// Creates the queue and starts watching the BOOT button:
// short press = Next, long press = ToggleHold.
void commandsBegin();

bool commandSend(Command command);

// Non-blocking; returns false when no command is waiting.
bool commandReceive(Command &command);
