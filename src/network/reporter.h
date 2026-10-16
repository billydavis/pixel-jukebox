#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Sends the panel's likes and view reports to Makapix from a background task
// on core 0, so the display never waits on the network. Likes go first, being
// what someone is waiting to see; views follow at no more than one every 5.5
// seconds, the most Makapix accepts. Anything that cannot be sent yet, offline
// say, waits: likes in the board's settings, views on the card when there is
// one (in memory otherwise).

// Call once, after cardBegin() and makapix::begin().
void reporterBegin();

// An artwork has been on the panel long enough to count as seen. `requested`:
// asked for by the user (the gallery) rather than picked by the rotation.
void reporterNoteView(uint32_t postId, bool requested);

bool reporterViewsEnabled();
// Turning reports off also drops any views still waiting.
void reporterSetViewsEnabled(bool enabled);

bool reporterLiked(uint32_t postId);
void reporterSetLiked(uint32_t postId, bool liked);
// The liked artworks, the most recent first.
std::vector<uint32_t> reporterLikes();

void reporterReportViews(JsonObject report);
void reporterReportLikes(JsonObject report);
