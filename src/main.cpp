// Pixel Jukebox - rotates through slots (Makapix artwork, clock) on a 64x64 LED matrix.

#include <Arduino.h>
#include <WiFi.h>

#include "audio.h"
#include "commands.h"
#include "config.h"
#include "display.h"
#include "pins.h"
#include "slot_clock.h"
#include "slot_makapix.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and fill in your Wi-Fi details"
#endif

static const uint32_t WIFI_TIMEOUT_MS = 20000;
static const uint32_t WIFI_RETRY_MS = 60000;
static const uint32_t MESSAGE_MS = 1500;
static const uint32_t SKIPPED_SLOT_MS = 200;
static const uint32_t COMMAND_POLL_MS = 5;

static MakapixSlot makapixSlot;
static ClockSlot clockSlot;

// Each slot is shown `turns` times in a row before moving on.
struct RotationEntry {
  Slot *slot;
  uint8_t turns;
};
static const RotationEntry ROTATION[] = {
    {&makapixSlot, 3},
    {&clockSlot, 1},
};
static const size_t ROTATION_LENGTH = sizeof(ROTATION) / sizeof(ROTATION[0]);

static size_t rotationIndex = 0;
static uint8_t turnsTaken = 0;
static size_t skippedInARow = 0;
static uint32_t lastWifiAttempt = 0;

static uint16_t amber() { return display->color565(255, 200, 0); }
static uint16_t red() { return display->color565(255, 0, 0); }
static uint16_t green() { return display->color565(0, 255, 0); }

static bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      return false;
    }
    delay(250);
  }
  return true;
}

// Reconnects when the connection is down, at most once per WIFI_RETRY_MS so
// slots that work offline keep their turns.
static void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (lastWifiAttempt != 0 && millis() - lastWifiAttempt < WIFI_RETRY_MS) {
    return;
  }
  lastWifiAttempt = millis();

  showMessage("WiFi", "joining...", amber());
  if (!connectWifi()) {
    Serial.println("Wi-Fi connection timed out");
    showMessage("WiFi", "failed", red());
    delay(MESSAGE_MS);
    return;
  }
  Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());
  configTzTime(TIMEZONE, NTP_SERVER);
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
        if (command == Command::Next) {
          Serial.println("Command: next");
          slot->stop();
          return;
        }
        held = !held;
        Serial.printf("Command: %s\n", held ? "hold" : "release");
        if (held) {
          drawHoldMarker();
        } else {
          slot->redraw(); // clears the marker
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

static size_t rotationTurns() {
  size_t turns = 0;
  for (const RotationEntry &entry : ROTATION) {
    turns += entry.turns;
  }
  return turns;
}

static void advanceRotation() {
  if (++turnsTaken >= ROTATION[rotationIndex].turns) {
    turnsTaken = 0;
    rotationIndex = (rotationIndex + 1) % ROTATION_LENGTH;
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

  commandsBegin();
  makapixSlot.begin();
}

void loop() {
  if (!display) {
    delay(1000);
    return;
  }

  maintainWifi();

  Slot *slot = ROTATION[rotationIndex].slot;
  if (slot->start()) {
    skippedInARow = 0;
    Serial.printf("Slot: %s\n", slot->name());
    runSlot(slot);
  } else {
    // Nothing to show yet (offline, still downloading, clock not set). Say so
    // once every slot has been skipped.
    if (++skippedInARow >= rotationTurns()) {
      skippedInARow = 0;
      showMessage("Pixel", "Jukebox\nwaiting...", amber());
      delay(MESSAGE_MS);
    }
    delay(SKIPPED_SLOT_MS);

    // Commands sent while nothing is showing have nothing to act on.
    Command discarded;
    while (commandReceive(discarded)) {
    }
  }
  advanceRotation();
}
