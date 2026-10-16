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
#include "app/night.h"
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
static bool nightClockShowing = false; // the clock is on because of the night schedule
static const uint32_t NIGHT_CHECK_MS = 500;

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

// True when the night schedule wants something other than `slot` on screen.
static bool nightWantsChange(Slot *slot) {
  NightNow now = nightNow();
  if (now.off) {
    return true;
  }
  if (modeIsSound()) {
    return false;
  }
  if (now.clockOnly) {
    return slot != rotationClockSlot();
  }
  return nightClockShowing; // the clock-only night is over
}

// Runs a started slot until it finishes or a command ends it.
static void runSlot(Slot *slot) {
  bool held = false;
  uint32_t nightCheckedAt = millis();
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
            nightWake(); // only does anything on a clock-only night
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
            nightSetBrightness(command.value);
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
      nightTick();
      if (millis() - nightCheckedAt >= NIGHT_CHECK_MS) {
        nightCheckedAt = millis();
        if (nightWantsChange(slot)) {
          statusSetHeld(false);
          slot->stop();
          return;
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
      nightSetBrightness(command.value);
    } else if (command.type == CommandType::WifiSetup) {
      connectionEnterSetup();
    } else if (command.type == CommandType::Restart) {
      restartBoard();
    } else if (command.type == CommandType::ShowArtwork && !modeIsSound()) {
      requestedPost = command.value;
    }
  }
}

// The night schedule has the panel dark: nothing plays, so nothing is
// downloaded or reported. Next, art from the gallery or a brightness change
// wakes it for a while.
static void nightDark() {
  statusSetSlot("");
  display->clearScreen();
  uint32_t until = millis() + 1000;
  while ((int32_t)(millis() - until) < 0) {
    Command command;
    while (commandReceive(command)) {
      switch (command.type) {
        case CommandType::ShowArtwork:
          requestedPost = command.value;
          nightWake();
          return;
        case CommandType::Next:
          nightWake();
          return;
        case CommandType::SetBrightness:
          nightSetBrightness(command.value);
          return;
        case CommandType::WifiSetup:
          connectionEnterSetup();
          break;
        case CommandType::Restart:
          restartBoard();
          break;
        default:
          break; // nothing to hold or reload while dark
      }
    }
    nightTick();
    if (!nightNow().off) {
      return;
    }
    delay(20);
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
  nightBegin();
  outgoing = (uint16_t *)ps_malloc(PANEL_RES_X * PANEL_RES_Y * sizeof(uint16_t));
}

void loop() {
  if (!display) {
    delay(1000);
    return;
  }

  connectionMaintain();
  clockChipMaintain();
  nightTick();
  NightNow night = nightNow();
  if (night.off) {
    nightClockShowing = false;
    nightDark();
    return;
  }

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
  // On a clock-only night the clock takes the place of the rotation.
  bool nightClock = !sound && !requested && night.clockOnly;
  Slot *slot = sound       ? modeSoundSlot()
               : requested ? requested
               : nightClock ? rotationClockSlot()
                            : rotationCurrent();

  // For a transition, the slot draws its first frame off the panel while the
  // old picture stays up, then the two are blended. Sound mode switches at
  // once, and the night clock does not blend into itself every ten seconds.
  bool animate = !sound && outgoing && transitionEnabled() && !(nightClock && nightClockShowing);
  nightClockShowing = nightClock;
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
  if (!sound && !requested && !nightClock) {
    rotationAdvance();
  }
}
