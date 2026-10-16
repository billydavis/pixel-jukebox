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
  String gifUrl; // HTTPS, for clients that want to show the artwork; empty when from the card
  bool fromCache = false;
  String channel; // label of the channel it came from; empty when from the card
};

void statusBegin();
void statusSetSlot(const char *name);
void statusSetHeld(bool held);
void statusSetArtwork(uint32_t postId, const String &owner, const String &gifUrl, bool fromCache,
                      const String &channel = "");
void statusClearArtwork();
PlayerStatus statusGet();
