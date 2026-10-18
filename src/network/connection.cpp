#include "network/connection.h"

#include <Arduino.h>
#include <WiFi.h>

#include "network/api.h"
#include "config.h"
#include "display/display.h"
#include "network/wifi_setup.h"
#include "storage/art_cache.h"

static const uint32_t JOIN_TIMEOUT_MS = 20000;
static const int BOOT_ATTEMPTS = 2;
// With art on the card, start-up waits this long before playing from it and
// joining in the background instead. A normal join takes a few seconds.
static const uint32_t QUICK_JOIN_MS = 8000;
static const uint32_t RETRY_MS = 60000;
static const uint32_t MESSAGE_MS = 1500;

static uint32_t lastAttempt = 0;

// Someone is at the panel setting up Wi-Fi: show the instructions at the day
// brightness even if the night schedule has it dark.
static void runSetup(bool fallback, bool requested) {
  displayApplyBrightness(displayBrightness());
  wifiSetupRun(fallback, requested);
}

void connectionStart() {
  wifiBegin();
  bool requested = wifiSetupRequested();
  if (requested || !wifiHasCredentials()) {
    runSetup(false, requested);
  }

  showMessage("WiFi", "joining...", colour::amber());
  uint32_t startedAt = millis();
  // With art on the card there is something to show without Wi-Fi, so after
  // one short attempt the panel plays it and keeps trying in the background:
  // when it fails, the router is more likely down than the password wrong.
  // Holding BOOT for five seconds still opens setup. Without a card it tries
  // longer, since the setup hotspot is the only thing left to offer.
  bool card = artCacheAvailable();
  int attempts = card ? 1 : BOOT_ATTEMPTS;
  for (int attempt = 0; attempt < attempts; attempt++) {
    if (wifiConnect(card ? QUICK_JOIN_MS : JOIN_TIMEOUT_MS)) {
      Serial.printf("Joined Wi-Fi in %lu ms\n", (unsigned long)(millis() - startedAt));
      return; // connectionMaintain() announces the connection
    }
  }
  if (card) {
    Serial.println("Could not join the saved network; playing from the card");
    showMessage("WiFi", "offline,\nplaying card", colour::amber());
    delay(MESSAGE_MS);
    lastAttempt = millis();
    wifiRetry();
    return;
  }
  Serial.println("Could not join the saved network");
  runSetup(true, false);
}

void connectionEnterSetup() {
  Serial.println("Command: Wi-Fi setup");
  showMessage("WiFi", "setup...", colour::amber());
  delay(MESSAGE_MS);
  wifiRequestSetup();
}

void connectionMaintain() {
  static bool announced = false;
  if (WiFi.status() != WL_CONNECTED) {
    announced = false;
    if (lastAttempt != 0 && millis() - lastAttempt < RETRY_MS) {
      return;
    }
    lastAttempt = millis();

    // Art on the card keeps playing, so retry in the background rather than
    // stopping the show for twenty seconds every minute.
    if (artCacheAvailable()) {
      Serial.println("Wi-Fi down; trying again in the background");
      wifiRetry();
      return;
    }

    showMessage("WiFi", "joining...", colour::amber());
    if (!wifiConnect(JOIN_TIMEOUT_MS)) {
      Serial.println("Wi-Fi connection timed out");
      showMessage("WiFi", "failed", colour::red());
      delay(MESSAGE_MS);
      return;
    }
  }
  if (announced) {
    return;
  }
  announced = true;
  Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());
  configTzTime(TIMEZONE, NTP_SERVER);
  apiBegin();
  showMessage("WiFi OK", WiFi.localIP().toString(), colour::green());
  delay(MESSAGE_MS);
}
