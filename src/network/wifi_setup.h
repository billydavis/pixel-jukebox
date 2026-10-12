#pragma once

#include <Arduino.h>

// Wi-Fi credentials and the first-time setup hotspot.
//
// Credentials come from the board's saved settings, or failing that from
// include/secrets.h if it was present at build time. With neither, or when the
// saved network cannot be joined at boot, the board starts its own open network
// (WIFI_SETUP_SSID) with a page for choosing a network and entering its password.

#define WIFI_SETUP_SSID "Pixel-Jukebox"

void wifiBegin();

bool wifiHasCredentials();

// Joins the stored network. Returns false if it is not connected in time.
bool wifiConnect(uint32_t timeoutMs);

// Restarts into the setup hotspot. The saved network is kept, and is used
// again if nobody completes setup.
void wifiRequestSetup();

// True once after wifiRequestSetup().
bool wifiSetupRequested();

// Runs the setup hotspot. Never returns: the board restarts once it has joined
// the chosen network and saved its details, or after a while with nobody
// connected if it has a network to retry. Nothing is saved for a network it
// could not join.
void wifiSetupRun(bool joinFailed);
