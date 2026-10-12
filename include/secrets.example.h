#pragma once

// Optional. Without a secrets.h the board starts a setup hotspot on first boot
// and asks for these on a web page. To skip that step, copy this file to
// secrets.h (git-ignored) and fill in your network.
// The ESP32-S3 only joins 2.4 GHz networks.

#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
