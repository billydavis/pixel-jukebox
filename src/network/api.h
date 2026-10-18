#pragma once

// REST API for remote control, documented in docs/jukebox-api.md, and the
// control page at "/" (web/index.html) that uses it.

// Starts the HTTP server and announces it over mDNS. Call once Wi-Fi is
// connected; later calls do nothing.
void apiBegin();

// True once the server is running and Wi-Fi is connected.
bool apiRunning();
