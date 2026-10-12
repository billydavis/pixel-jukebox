// Pixel Jukebox - rotates through slots (Makapix artwork, clock) on a 64x64 LED matrix.
// The rotation itself lives in rotation.cpp.

#include <Arduino.h>
#include <WiFi.h>

#include "api.h"
#include "audio.h"
#include "commands.h"
#include "config.h"
#include "display.h"
#include "pins.h"
#include "rotation.h"
#include "status.h"
#include "wifi_setup.h"

static const uint32_t WIFI_TIMEOUT_MS = 20000;
static const int WIFI_BOOT_ATTEMPTS = 2;
static const uint32_t WIFI_RETRY_MS = 60000;
static const uint32_t MESSAGE_MS = 1500;
static const uint32_t SKIPPED_SLOT_MS = 200;
static const uint32_t COMMAND_POLL_MS = 5;

static size_t skippedInARow = 0;
static uint32_t lastWifiAttempt = 0;

static uint16_t amber() { return display->color565(255, 200, 0); }
static uint16_t red() { return display->color565(255, 0, 0); }
static uint16_t green() { return display->color565(0, 255, 0); }

// Joins Wi-Fi at boot. With no network to join, or one that cannot be joined,
// this hands over to the setup hotspot, which ends in a restart.
static void startWifi() {
  wifiBegin();
  if (wifiSetupRequested() || !wifiHasCredentials()) {
    wifiSetupRun(false);
  }

  showMessage("WiFi", "joining...", amber());
  for (int attempt = 0; attempt < WIFI_BOOT_ATTEMPTS; attempt++) {
    if (wifiConnect(WIFI_TIMEOUT_MS)) {
      return; // maintainWifi() announces the connection
    }
  }
  Serial.println("Could not join the saved network");
  wifiSetupRun(true);
}

// Restarts into the setup hotspot when asked by the button or the API.
static void enterWifiSetup() {
  Serial.println("Command: Wi-Fi setup");
  showMessage("WiFi", "setup...", amber());
  delay(MESSAGE_MS);
  wifiRequestSetup();
}

// Announces a new connection and reconnects when it drops, at most once per
// WIFI_RETRY_MS so slots that work offline keep their turns.
static void maintainWifi() {
  static bool announced = false;
  if (WiFi.status() != WL_CONNECTED) {
    announced = false;
    if (lastWifiAttempt != 0 && millis() - lastWifiAttempt < WIFI_RETRY_MS) {
      return;
    }
    lastWifiAttempt = millis();

    showMessage("WiFi", "joining...", amber());
    if (!wifiConnect(WIFI_TIMEOUT_MS)) {
      Serial.println("Wi-Fi connection timed out");
      showMessage("WiFi", "failed", red());
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
  showMessage("WiFi OK", WiFi.localIP().toString(), green());
  delay(MESSAGE_MS);
}

// Two small bars in the top-right corner while the current slot is held.
static void drawHoldMarker() {
  uint16_t white = display->color565(255, 255, 255);
  display->fillRect(PANEL_RES_X - 6, 1, 2, 5, white);
  display->fillRect(PANEL_RES_X - 3, 1, 2, 5, white);
}

// Runs a started slot until it finishes or a command ends it.
static void runSlot(Slot *slot) {
  bool held = false;
  for (;;) {
    uint32_t updatedAt = millis();
    uint32_t waitMs = slot->update();
    if (held) {
      drawHoldMarker();
    }

    do {
      Command command;
      while (commandReceive(command)) {
        bool wantHeld = held;
        switch (command.type) {
          case CommandType::Next:
            Serial.println("Command: next");
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::ToggleHold:
            wantHeld = !held;
            break;
          case CommandType::SetHold:
            wantHeld = command.value != 0;
            break;
          case CommandType::WifiSetup:
            enterWifiSetup();
            break;
          case CommandType::SetBrightness:
            Serial.printf("Command: brightness %u\n", command.value);
            displaySetBrightness(command.value);
            break;
        }
        if (wantHeld != held) {
          held = wantHeld;
          statusSetHeld(held);
          Serial.printf("Command: %s\n", held ? "hold" : "release");
          if (held) {
            drawHoldMarker();
          } else {
            slot->redraw(); // clears the marker
          }
        }
      }
      delay(COMMAND_POLL_MS);
    } while (millis() - updatedAt < waitMs);

    if (!held && slot->finished()) {
      slot->stop();
      return;
    }
  }
}

#ifdef SPEAKER_TEST
// Lists the I2C devices, then plays a rising chime and a steady tone three times.
static void runSpeakerTest() {
  String devices = audioScanI2c();
  Serial.printf("I2C devices: %s\n", devices.c_str());
  showMessage("I2C", devices, amber());
  delay(4000);

  if (!audioBegin()) {
    Serial.println("Audio init failed");
    showMessage("Speaker", "init failed", red());
    delay(5000);
    return;
  }
  Serial.println("Audio init OK, playing tones");
  showMessage("Speaker", "playing", green());
  const float chime[] = {523.25f, 659.25f, 783.99f, 1046.5f}; // C5 E5 G5 C6
  for (int repeat = 0; repeat < 3; repeat++) {
    for (float note : chime) {
      audioTone(note, 250);
    }
    delay(300);
    audioTone(1000, 1000);
    delay(700);
  }
}
#endif

void setup() {
  Serial.begin(115200);

  if (!setupDisplay()) {
    Serial.println("Display init failed (DMA buffer allocation)");
    return;
  }
  drawTestPattern();
  delay(1500);
#ifdef COLOUR_CHECK
  runColourCheck();
#endif
#ifdef SPEAKER_TEST
  runSpeakerTest();
#endif

  startWifi();

  statusBegin();
  commandsBegin();
  rotationBegin();
}

void loop() {
  if (!display) {
    delay(1000);
    return;
  }

  maintainWifi();

  Slot *slot = rotationCurrent();
  if (slot->start()) {
    skippedInARow = 0;
    statusSetSlot(slot->name());
    Serial.printf("Slot: %s\n", slot->name());
    runSlot(slot);
    statusSetSlot("");
  } else {
    // Nothing to show yet (offline, still downloading, clock not set). Say so
    // once every slot has been skipped.
    if (++skippedInARow >= rotationTurns()) {
      skippedInARow = 0;
      showMessage("Pixel", "Jukebox\nwaiting...", amber());
      delay(MESSAGE_MS);
    }
    delay(SKIPPED_SLOT_MS);

    // With nothing showing, hold and next have nothing to act on.
    Command command;
    while (commandReceive(command)) {
      if (command.type == CommandType::SetBrightness) {
        displaySetBrightness(command.value);
      } else if (command.type == CommandType::WifiSetup) {
        enterWifiSetup();
      }
    }
  }
  rotationAdvance();
}
