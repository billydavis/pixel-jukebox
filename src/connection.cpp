#include "connection.h"

#include <Arduino.h>
#include <WiFi.h>

#include "api.h"
#include "config.h"
#include "display.h"
#include "wifi_setup.h"

static const uint32_t JOIN_TIMEOUT_MS = 20000;
static const int BOOT_ATTEMPTS = 2;
static const uint32_t RETRY_MS = 60000;
static const uint32_t MESSAGE_MS = 1500;

static uint32_t lastAttempt = 0;

void connectionStart() {
  wifiBegin();
  if (wifiSetupRequested() || !wifiHasCredentials()) {
    wifiSetupRun(false);
  }

  showMessage("WiFi", "joining...", colour::amber());
  for (int attempt = 0; attempt < BOOT_ATTEMPTS; attempt++) {
    if (wifiConnect(JOIN_TIMEOUT_MS)) {
      return; // connectionMaintain() announces the connection
    }
  }
  Serial.println("Could not join the saved network");
  wifiSetupRun(true);
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
