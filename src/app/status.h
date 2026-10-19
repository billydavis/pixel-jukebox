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
  String screen;  // what has the panel: screen::name() of the current source
  uint32_t uploadId = 0; // an upload is showing (postId is 0 then)
  String uploadName;
  uint16_t starterId = 0; // a piece of starter art is showing (postId is 0 then)
  String starterTitle;
  String starterCredit;
};

void statusBegin();
void statusSetSlot(const char *name);
void statusSetHeld(bool held);
void statusSetScreen(const char *name);
void statusSetArtwork(uint32_t postId, const String &owner, const String &gifUrl, bool fromCache,
                      const String &channel = "");
// An upload (storage/uploads.h) in place of a Makapix artwork.
void statusSetUpload(uint32_t uploadId, const String &name);
// Starter art (storage/starter_art.h). `fallback`: shown because there was no
// Makapix art, rather than from the starter channel.
void statusSetStarter(uint16_t id, const String &title, const String &credit, bool fallback);
void statusClearArtwork();
PlayerStatus statusGet();
