#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Art uploaded from a phone or PC (POST /api/v1/uploads): GIFs of 64 x 64 or
// less, kept on the card in their own folder, apart from the art cache, so
// they are never evicted and play without Makapix or Wi-Fi. The "uploads"
// channel plays them. Each file's name carries its id, size and name
// (upload_rules.h), so start-up reads the folder once and opens nothing.
// Every call takes the card lock; safe from any task.

struct UploadInfo {
  uint32_t id;
  String name;
  uint16_t width;
  uint16_t height;
  uint32_t bytes;
};

void uploadsBegin();

bool uploadsAvailable(); // a card is in and the folder could be made
std::vector<UploadInfo> uploadsList(); // newest first
bool uploadsFind(uint32_t id, UploadInfo &info);

// The GIF in PSRAM, with room after it for small-GIF padding (gif_pad.h);
// the caller frees it.
bool uploadsRead(uint32_t id, uint8_t *&data, size_t &size);

// The next upload for the uploads channel: each once, in a shuffled order,
// then a new order. False when there are none.
bool uploadsNext(uint32_t &id);

bool uploadsRemove(uint32_t id);

// Receiving one, a piece at a time as the request body arrives. start()
// checks the space and the count; the first piece is checked to be a GIF of
// the right size before anything is kept. finish() makes it part of the list
// (written under a temporary name until then, so a cut-off upload leaves
// nothing behind). False with `error` at any step; then abort().
bool uploadsStart(size_t length, const String &name, String &error);
bool uploadsWrite(const uint8_t *data, size_t length, String &error);
bool uploadsFinish(UploadInfo &info, String &error);
void uploadsAbort();

void uploadsReport(JsonObject out);
