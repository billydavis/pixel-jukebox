#pragma once

#include <Arduino.h>

// What the panel is doing right now. Written by the display loop, read by the
// HTTP API; every call is safe from any task.
struct PlayerStatus {
  String slot; // empty while nothing is showing
  bool held = false;
  bool hasArtwork = false;
  uint32_t postId = 0;
  String owner;
};

void statusBegin();
void statusSetSlot(const char *name);
void statusSetHeld(bool held);
void statusSetArtwork(uint32_t postId, const String &owner);
void statusClearArtwork();
PlayerStatus statusGet();
