// Pixel Jukebox - shows Makapix artwork, a clock and sound visualizers on a
// 64x64 LED matrix.
//
// This file brings the board up and runs one slot after another. What is
// chosen to run comes from rotation.cpp and mode.cpp, how one gives way to
// the next from transition.cpp, and Wi-Fi is kept up by connection.cpp.

#include <Arduino.h>

#include "app/clock_chip.h"
#include "app/commands.h"
#include "app/crash_report.h"
#include "app/diagnostics.h"
#include "display/display.h"
#include "app/mode.h"
#include "network/connection.h"
#include "network/reporter.h"
#include "pins.h"
#include "app/rotation.h"
#include "app/status.h"
#include "app/telemetry.h"
#include "storage/art_cache.h"
#include "storage/card.h"
#include "app/transition.h"
#include "slots/slot_makapix.h"

static const uint32_t MESSAGE_MS = 1500;
static const uint32_t SKIPPED_SLOT_MS = 200;
static const uint32_t COMMAND_POLL_MS = 5;

static size_t skippedInARow = 0;
static uint16_t *outgoing = nullptr; // the frame a transition starts from
static uint32_t requestedPost = 0;   // an artwork asked for from the gallery

// The slot has no card-detect switch, so a card is only looked for at start-up.
static void restartBoard() {
  Serial.println("Command: restart");
  showMessage("Pixel", "Jukebox\nrestarting", colour::amber());
  delay(MESSAGE_MS);
  ESP.restart();
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
            if (modeIsSound()) {
              modeNextVisualizer();
            }
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::Reload:
            statusSetHeld(false);
            slot->stop();
            return;
          // A visualizer already stays put, so hold only applies to the rotation.
          case CommandType::ToggleHold:
            wantHeld = !held && !modeIsSound();
            break;
          case CommandType::SetHold:
            wantHeld = command.value != 0 && !modeIsSound();
            break;
          case CommandType::WifiSetup:
            connectionEnterSetup();
            break;
          case CommandType::Restart:
            restartBoard();
            break;
          case CommandType::ShowArtwork:
            if (modeIsSound()) {
              break;
            }
            Serial.printf("Command: show post %u\n", (unsigned)command.value);
            requestedPost = command.value;
            statusSetHeld(false);
            slot->stop();
            return;
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

// With nothing on screen, hold and next have nothing to act on.
static void handleCommandsWhileIdle() {
  Command command;
  while (commandReceive(command)) {
    if (command.type == CommandType::SetBrightness) {
      displaySetBrightness(command.value);
    } else if (command.type == CommandType::WifiSetup) {
      connectionEnterSetup();
    } else if (command.type == CommandType::Restart) {
      restartBoard();
    } else if (command.type == CommandType::ShowArtwork && !modeIsSound()) {
      requestedPost = command.value;
    }
  }
}

void setup() {
  Serial.begin(115200);

  if (!setupDisplay()) {
    Serial.println("Display init failed (DMA buffer allocation)");
    return;
  }
  drawTestPattern();
  delay(1500);
  runDiagnostics();
  clockChipBegin(); // the time, before Wi-Fi
  crashBegin();     // after the clock, to note when a crash record was found
  cardBegin();
  artCacheBegin();

  telemetryRegisterTask("display", xTaskGetCurrentTaskHandle(), getArduinoLoopTaskStackSize());
  connectionStart();

  statusBegin();
  commandsBegin();
  rotationBegin();
  reporterBegin(); // after rotationBegin(), which starts the Makapix client
  modeBegin();
  transitionBegin();
  outgoing = (uint16_t *)ps_malloc(PANEL_RES_X * PANEL_RES_Y * sizeof(uint16_t));
}

void loop() {
  if (!display) {
    delay(1000);
    return;
  }

  connectionMaintain();
  clockChipMaintain();

  // Sound mode shows one visualizer until the mode changes; otherwise the
  // rotation decides.
  bool sound = modeIsSound();
  // An artwork asked for from the gallery plays between turns of the rotation,
  // which then carries on where it was.
  MakapixSlot *requested = nullptr;
  if (!sound && requestedPost) {
    requested = rotationArtworkSlot();
    requested->request(requestedPost);
    requestedPost = 0;
  }
  Slot *slot = sound ? modeSoundSlot() : requested ? requested : rotationCurrent();

  // For a transition, the slot draws its first frame off the panel while the
  // old picture stays up, then the two are blended. Sound mode switches at once.
  bool animate = !sound && outgoing && transitionEnabled();
  if (animate) {
    memcpy(outgoing, display->pixels(), PANEL_RES_X * PANEL_RES_Y * sizeof(uint16_t));
    display->setLive(false);
  }
  bool started = slot->start();
  if (animate) {
    if (started) {
      slot->update();
      transitionPlay(outgoing, display->pixels());
    }
    display->setLive(true);
  }

  if (started) {
    slot->shown();
    skippedInARow = 0;
    statusSetSlot(slot->name());
    Serial.printf("Slot: %s\n", slot->name());
    runSlot(slot);
    statusSetSlot("");
  } else if (requested) {
    // Removed from the card since it was asked for: carry on with the rotation.
  } else if (sound) {
    // The microphones are unavailable, so go back to the rotation.
    Serial.println("Sound mode could not start");
    String error;
    modeSet(false, "", error);
    showMessage("Sound", "unavailable", colour::red());
    delay(MESSAGE_MS);
  } else {
    // Nothing to show yet (offline, still downloading, clock not set). Say so
    // once every slot has been skipped.
    if (++skippedInARow >= rotationTurns()) {
      skippedInARow = 0;
      showMessage("Pixel", "Jukebox\nwaiting...", colour::amber());
      delay(MESSAGE_MS);
    }
    delay(SKIPPED_SLOT_MS);
    handleCommandsWhileIdle();
  }
  if (!sound && !requested) {
    rotationAdvance();
  }
}
