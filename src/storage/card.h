#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// The optional microSD card. With no card, or one that cannot be read, every
// feature that uses it is simply off. A card is never formatted by the
// firmware: it must already hold a FAT file system (FAT12, FAT16 or FAT32),
// and only the jukebox/ folder on it is touched.

// Files on the card live under here.
#define CARD_ROOT "/sdcard/jukebox"

// Looks for a card and mounts it. Call once at start-up. The slot has no
// card-detect switch, so a card inserted later is only seen after a restart.
void cardBegin();

bool cardReady();

// Present, size, free space and, when it cannot be used, why.
void cardReport(JsonObject report);
