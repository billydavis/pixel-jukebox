#include "app/settings_reset.h"

#include <Arduino.h>
#include <Preferences.h>

// Every settings namespace but "crash" (see the header), and "jukebox", which
// also holds the Wi-Fi details and is cleared below with those put back.
static const char *const CLEARED[] = {"auth", "clock", "live", "makapix", "night", "reports", "weather"};

void settingsResetAll() {
  Preferences prefs;
  for (const char *name : CLEARED) {
    if (prefs.begin(name, false)) {
      prefs.clear();
      prefs.end();
    }
  }
  // The rotation, brightness, photo mode, orientation, transition and
  // visualizer share "jukebox" with the Wi-Fi details.
  if (prefs.begin("jukebox", false)) {
    String ssid = prefs.getString("wifi_ssid", "");
    String password = prefs.getString("wifi_pass", "");
    prefs.clear();
    if (!ssid.isEmpty()) {
      prefs.putString("wifi_ssid", ssid);
      prefs.putString("wifi_pass", password);
    }
    prefs.end();
  }
  Serial.println("Settings: all reset to their defaults, Wi-Fi kept");
}
