#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// The live connection to makapix.club (MQTT over mutual TLS, port 8883).
// With it the panel shows as online on the website, with the artwork it is
// showing, and the website can control it: Next, Back, "send to player" and
// play a channel. Everything else (art, likes, views) still goes over HTTPS.
//
// It runs in its own task once the panel is linked and Wi-Fi is up, and
// reconnects with growing pauses (1 s to 5 min) when the connection drops.
// The panel's MQTT certificate is fetched with its token, kept in flash, and
// renewed when it has 30 days left or the broker keeps turning it away.
// Every TLS read and write holds the card lock (storage/card.h).
//
// While the night schedule has the panel off it is offline too: it says
// goodbye to the broker and connects again when night ends or the panel is
// woken from the control page or the button.
//
// On by default; can be switched off (PUT /api/v1/live), which says goodbye
// to the broker and stays off across restarts.

void liveBegin();

bool liveEnabled();
void liveSetEnabled(bool enabled);

// For GET /api/v1/live and /diagnostics.
void liveReport(JsonObject out);
