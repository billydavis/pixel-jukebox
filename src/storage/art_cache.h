#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// A copy on the card of the Makapix artworks the board has played, so it has
// something to show without a connection. Off when there is no usable card.
// The card is read and written by the artwork loader task, by the API for the
// gallery, and by the display loop when an artwork is asked for from the
// gallery. A lock keeps them apart; none holds it for longer than one file.

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

// Reads one artwork to play it now, as asked for from the gallery. Counts as
// played. False when it is not cached or cannot be read.
bool artCacheTakePost(uint32_t postId, CachedArtwork &artwork);

// Reads one artwork's file without counting it as played, for the gallery.
// `data` is PSRAM, freed by the caller.
bool artCacheReadFile(uint32_t postId, uint8_t *&data, size_t &size);

bool artCacheHas(uint32_t postId);

// Up to `limit` artworks from `offset`, the most recently played first.
void artCacheList(uint32_t offset, uint32_t limit, JsonObject out);

// Deletes one artwork from the card. False when it was not cached.
bool artCacheRemove(uint32_t postId);

// Deletes every cached artwork. Returns how many there were.
int artCacheClear();

void artCacheReport(JsonObject report);
