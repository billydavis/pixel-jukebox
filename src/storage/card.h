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

// Hold one of these while opening, reading, writing, renaming or removing
// anything on the card; Makapix requests hold it too (makapix.cpp).
//
// Why: a picture being read from the card for the API was found closed under
// it, crashing the board (LoadProhibited in FatFs validate(), crash records of
// 2026-10-16 and 2026-10-17). It was reproduced by fetching gallery pictures
// while view reports went out, about once a minute, and bisected: it needs a
// TLS request in one task while another task has a card file open, and
// neither card writes nor anything else in the reporter was required. The
// cause is inside the prebuilt framework (FatFs, the network stack or the
// crypto hardware), so the cure is to never let the two overlap: card access
// and TLS requests take turns behind this lock. The lock is recursive: code
// already holding it may take it again.
class CardLock {
public:
  CardLock();
  ~CardLock();
  CardLock(const CardLock &) = delete;
  CardLock &operator=(const CardLock &) = delete;
};
