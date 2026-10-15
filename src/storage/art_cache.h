#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// A copy on the card of the Makapix artworks the board has played, so it has
// something to show without a connection. Off when there is no usable card.
// Card access happens only in the artwork loader task, apart from start-up.

// Reads the cache's log from the card. Call once at start-up, after cardBegin().
void artCacheBegin();

// Saves an artwork just downloaded, or notes that a cached one played again,
// making room by removing whatever has gone longest unplayed. Never fails
// loudly: a card problem only means the artwork is not cached.
void artCacheStore(uint32_t postId, const String &owner, uint32_t dwellMs, const uint8_t *data, size_t size);

// True when there is at least one artwork to play without a connection.
bool artCacheAvailable();

struct CachedArtwork {
  uint32_t postId;
  String owner;
  uint32_t dwellMs;
  uint8_t *data; // PSRAM, freed by the caller
  size_t size;
};

// Reads the next artwork in a shuffled pass through the cache. An artwork that
// cannot be read is removed and the next one tried. Loader task only.
bool artCacheTake(CachedArtwork &artwork);

void artCacheReport(JsonObject report);
