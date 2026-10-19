#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// The starter art built into the firmware (backlog #28): 64x64 GIFs from CC0
// pixel art packs and some drawn by code, listed in CREDITS.md. It plays on
// the "starter" channel, and in place of Makapix art when there is none to
// show: before the panel is linked, or offline with nothing on the card.
// Read-only, so safe from any task once starterArtBegin() has run.

struct StarterPiece {
  uint16_t id; // 1 up, in the order of assets/starter/manifest.json
  String title;
  String credit;
};

void starterArtBegin();

size_t starterArtCount();
bool starterArtFind(uint16_t id, StarterPiece &piece);

// The GIF as it is in flash, for serving it as it is; with `still`, its
// one-frame still, for thumbnails.
bool starterArtBytes(uint16_t id, const uint8_t *&data, size_t &size, bool still = false);

// A copy in PSRAM, with room after it for small-GIF padding (gif_pad.h), to
// play like any other artwork; the caller frees it.
bool starterArtRead(uint16_t id, uint8_t *&data, size_t &size);

// The next piece for the starter channel: each once, in a shuffled order, then
// a new order. False when there are none. Display loop and loader only.
bool starterArtNext(uint16_t &id);

void starterArtReport(JsonObject out);
