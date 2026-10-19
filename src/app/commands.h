#pragma once

#include <Arduino.h>

// Commands for the display loop. Anything may send them (the BOOT button, the
// HTTP API); only the display loop receives them.
enum class CommandType : uint8_t {
  Next,          // skip to the next slot
  ToggleHold,    // stay on the current slot, or release it
  SetHold,       // value: 1 = hold, 0 = release
  SetBrightness, // value: 0-255
  WifiSetup,     // restart into the Wi-Fi setup hotspot
  Reload,        // the mode changed: end the current slot and start the right one
  Restart,       // restart the board, for instance after a card is inserted
  ShowArtwork,   // value: a postId to show for one turn, from the card or Makapix
  Back,          // show the artwork before the current one
  ShowUpload,    // value: an upload's id (storage/uploads.h), to show for one turn
  ShowStarter,   // value: a starter art id (storage/starter_art.h), to show for one turn
  ShowTestPattern, // value: a testpattern::Pattern, shown until Next or ten minutes
};

struct Command {
  CommandType type;
  uint32_t value;
};

// Creates the queue and starts watching the BOOT button:
// short press = Next, long press = ToggleHold, held for 5 seconds = WifiSetup.
void commandsBegin();

bool commandSend(CommandType type, uint32_t value = 0);

// Non-blocking; returns false when no command is waiting.
bool commandReceive(Command &command);

// Looks at the next command without taking it.
bool commandPeek(Command &command);
