#pragma once

#include <Arduino.h>

// Which transition plays between slots in the rotation, and playing it. The
// choice is "random" (the default, never the same twice running), "none", or
// the name of one transition, and is saved on the board. The effects
// themselves are in lib/jukebox_core/transitions.

void transitionBegin();

// Safe from any task.
String transitionChoice();
bool transitionSetChoice(const String &choice, String &error);

// False when the choice is "none".
bool transitionEnabled();

// Animates the panel from `from` to `to` (both 64x64 RGB565) and leaves it
// showing `to`. A Next command arriving part-way cuts straight to the end and
// is used up. Call from the display loop only.
void transitionPlay(const uint16_t *from, const uint16_t *to);
