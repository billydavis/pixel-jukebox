#pragma once

#include <Arduino.h>
#include <vector>

#include "rotation.h"
#include "slot.h"

// What the panel is doing overall: working through the rotation, or showing one
// sound visualizer until told otherwise. The panel always starts in the
// rotation; the last visualizer chosen is remembered. Every call is safe from
// any task.

void modeBegin();

const std::vector<SlotInfo> &modeVisualizers();

bool modeIsSound();
String modeVisualizer();

// Switches mode and, in sound mode, picks the visualizer (empty keeps the
// current one). Interrupts whatever is on screen. On failure nothing changes
// and `error` says why.
bool modeSet(bool sound, const String &visualizer, String &error);

// For the display loop: the chosen visualizer's slot, and stepping to the next one.
Slot *modeSoundSlot();
void modeNextVisualizer();
