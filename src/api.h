#pragma once

// REST API for remote control, documented in docs/jukebox-api.md.

// Starts the HTTP server and announces it over mDNS. Call once Wi-Fi is
// connected; later calls do nothing.
void apiBegin();
