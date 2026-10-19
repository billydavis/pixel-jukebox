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
#include "app/memory_trace.h"
#include "app/diagnostics.h"
#include "app/firmware_update.h"
#include "display/display.h"
#include "input/case_climate.h"
#include "app/mode.h"
#include "app/night.h"
#include "network/auth.h"
#include "network/api.h"
#include "network/connection.h"
#include "network/live.h"
#include "network/reporter.h"
#include "pins.h"
#include "app/rotation.h"
#include "app/status.h"
#include "app/telemetry.h"
#include "storage/art_cache.h"
#include "storage/card.h"
#include "storage/starter_art.h"
#include "storage/uploads.h"
#include "app/transition.h"
#include "screen_rules.h"
#include "audio/audio.h"
#include "slots/slot_makapix.h"
#include "slots/slot_test.h"

static const uint32_t MESSAGE_MS = 1500;
static const uint32_t SKIPPED_SLOT_MS = 200;
static const uint32_t COMMAND_POLL_MS = 5;

static size_t skippedInARow = 0;
static uint16_t *outgoing = nullptr; // the frame a transition starts from
static uint32_t requestedPost = 0;   // an artwork asked for (gallery, Back, makapix.club)
static uint32_t requestedUpload = 0; // an upload asked for from the Art tab
static uint16_t requestedStarter = 0; // a piece of starter art asked for from the Art tab
static int requestedTest = -1;       // a test pattern asked for from Settings
static TestPatternSlot testSlot;
static bool requestFromBack = false; // that request came from Back: not a new step in the history
static screen::Source lastSource = screen::Source::Rotation;
static screen::History history;
static const uint32_t NIGHT_CHECK_MS = 500;

// The slot has no card-detect switch, so a card is only looked for at start-up.
static void restartBoard() {
  Serial.println("Command: restart");
  showMessage("Pixel", "Jukebox\nrestarting", colour::amber());
  delay(MESSAGE_MS);
  ESP.restart();
}

// While new firmware arrives the panel shows how far it has got instead of art.
static void showUpdateProgress() {
  static int shown = -1;
  int percent = firmwareUpdatePercent();
  if (percent != shown) {
    shown = percent;
    statusSetSlot("");
    showMessage("Update", "receiving\n" + String(percent) + "%", colour::amber());
  }
  delay(100);
}

// Two small bars in the top-right corner while the current slot is held.
static void drawHoldMarker() {
  uint16_t white = display->color565(255, 255, 255);
  display->fillRect(PANEL_RES_X - 6, 1, 2, 5, white);
  display->fillRect(PANEL_RES_X - 3, 1, 2, 5, white);
}

// Turns or mirrors the picture (a packed orientation.h value) and saves it.
static void setOrientation(uint32_t packed) {
  orientation::Orientation o = orientation::unpack(packed);
  Serial.printf("Command: orientation %d degrees%s\n", orientation::toDegrees(o), o.mirror ? ", mirrored" : "");
  displaySetOrientation(o);
}

// What screen_rules decides from: the night schedule, the mode, a request.
static screen::Inputs screenInputs(bool requestPending) {
  NightNow night = nightNow();
  screen::Inputs inputs;
  inputs.nightOff = night.off;
  inputs.nightClock = night.clockOnly;
  inputs.sound = modeIsSound();
  inputs.requestPending = requestPending;
  return inputs;
}

// Back: the artwork before the current one, shown for a turn. False when
// there is none.
static bool requestPrevious() {
  uint32_t postId = history.back();
  if (!postId) {
    return false;
  }
  Serial.printf("Command: back to post %u\n", (unsigned)postId);
  requestedPost = postId;
  requestFromBack = true;
  return true;
}

// Runs a started slot until it finishes or a command ends it. `source` is
// what put it on screen, which decides what Next and Hold mean.
static void runSlot(Slot *slot, screen::Source source) {
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
            if (screen::nextChangesVisualizer(source)) {
              modeNextVisualizer();
            }
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::Reload:
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::ToggleHold:
            wantHeld = !held && screen::holdAllowed(source);
            break;
          case CommandType::SetHold:
            wantHeld = command.value != 0 && screen::holdAllowed(source);
            break;
          case CommandType::Back:
            if (requestPrevious()) {
              statusSetHeld(false);
              slot->stop();
              return;
            }
            break;
          case CommandType::WifiSetup:
            connectionEnterSetup();
            break;
          case CommandType::Restart:
            restartBoard();
            break;
          case CommandType::ShowArtwork:
            Serial.printf("Command: show post %u\n", (unsigned)command.value);
            requestedPost = command.value;
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::ShowUpload:
            Serial.printf("Command: show upload %u\n", (unsigned)command.value);
            requestedUpload = command.value;
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::ShowStarter:
            Serial.printf("Command: show starter art %u\n", (unsigned)command.value);
            requestedStarter = command.value;
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::ShowTestPattern:
            Serial.printf("Command: test pattern %u\n", (unsigned)command.value);
            requestedTest = command.value;
            statusSetHeld(false);
            slot->stop();
            return;
          case CommandType::SetBrightness:
            Serial.printf("Command: brightness %u\n", command.value);
            nightSetBrightness(command.value);
            break;
          case CommandType::SetOrientation:
            setOrientation(command.value);
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
      // A visualizer or a held slot can stay on for hours, so a new firmware
      // is kept from in here too, not only between slots.
      firmwareUpdateMaintain(apiRunning());
      if (firmwareUpdateActive()) {
        statusSetHeld(false);
        slot->stop();
        return;
      }
      if (millis() - nightCheckedAt >= NIGHT_CHECK_MS) {
        nightCheckedAt = millis();
        if (screen::shouldGiveWay(source, screenInputs(false))) {
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

// While a slot is waiting for a download: brightness, restart and Wi-Fi
// setup are acted on at once; anything that changes what is on screen waits
// in the queue for the slot.
static void serviceWhileWaiting() {
  Command command;
  while (commandPeek(command)) {
    if (command.type != CommandType::SetBrightness && command.type != CommandType::SetOrientation &&
        command.type != CommandType::Restart && command.type != CommandType::WifiSetup) {
      return;
    }
    commandReceive(command);
    if (command.type == CommandType::SetBrightness) {
      Serial.printf("Command: brightness %u\n", command.value);
      nightSetBrightness(command.value);
    } else if (command.type == CommandType::SetOrientation) {
      setOrientation(command.value);
    } else if (command.type == CommandType::Restart) {
      restartBoard();
    } else {
      connectionEnterSetup();
    }
  }
  nightTick();
  firmwareUpdateMaintain(apiRunning());
}

// With nothing on screen, hold and next have nothing to act on.
static void handleCommandsWhileIdle() {
  Command command;
  while (commandReceive(command)) {
    if (command.type == CommandType::SetBrightness) {
      nightSetBrightness(command.value);
    } else if (command.type == CommandType::SetOrientation) {
      setOrientation(command.value);
    } else if (command.type == CommandType::WifiSetup) {
      connectionEnterSetup();
    } else if (command.type == CommandType::Restart) {
      restartBoard();
    } else if (command.type == CommandType::ShowArtwork) {
      requestedPost = command.value;
    } else if (command.type == CommandType::ShowUpload) {
      requestedUpload = command.value;
    } else if (command.type == CommandType::ShowStarter) {
      requestedStarter = command.value;
    } else if (command.type == CommandType::ShowTestPattern) {
      requestedTest = command.value;
    } else if (command.type == CommandType::Back) {
      requestPrevious();
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
        case CommandType::ShowUpload:
          requestedUpload = command.value;
          nightWake();
          return;
        case CommandType::ShowStarter:
          requestedStarter = command.value;
          nightWake();
          return;
        case CommandType::ShowTestPattern:
          requestedTest = command.value;
          nightWake();
          return;
        case CommandType::Next:
          nightWake();
          return;
        case CommandType::Back:
          if (requestPrevious()) {
            nightWake();
            return;
          }
          break;
        case CommandType::SetBrightness:
          nightSetBrightness(command.value);
          return;
        case CommandType::SetOrientation:
          setOrientation(command.value); // saved; the panel is dark, so nothing shows yet
          break;
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
  MEMORY_AT("start");
  telemetryBootMark("start");

  if (!setupDisplay()) {
    Serial.println("Display init failed (DMA buffer allocation)");
    return;
  }
  MEMORY_AT("display");
  telemetryBootMark("display");
  // The time and the night schedule come before anything is drawn, so a
  // restart at night does not flash the room: the panel starts at the night
  // level (dark when it is off) and skips the test pattern.
  clockChipBegin();
  nightBegin();
  nightTick();
  NightNow night = nightNow();
  if (night.night && !night.overridden) {
    Serial.printf("Night: starting at brightness %u, no test pattern\n", night.brightness);
  } else {
    drawTestPattern();
    delay(1500);
  }
  runDiagnostics();
  crashBegin(); // after the clock, to note when a crash record was found
  caseClimateBegin();
  MEMORY_AT("clock, crash, sensor");
  telemetryBootMark("clock, crash, sensor");
  cardBegin();
  MEMORY_AT("card");
  telemetryBootMark("card");
  artCacheBegin();
  uploadsBegin();
  starterArtBegin();
  MEMORY_AT("cache");
  telemetryBootMark("cache");

  telemetryRegisterTask("display", xTaskGetCurrentTaskHandle(), getArduinoLoopTaskStackSize());
  firmwareUpdateBegin();
  authBegin(); // before Wi-Fi: the setup page can remove the PIN
  connectionStart();
  MEMORY_AT("wifi, api");
  telemetryBootMark("wifi, api");

  statusBegin();
  commandsBegin();
  rotationBegin();
  MEMORY_AT("rotation");
  telemetryBootMark("rotation");
  reporterBegin(); // after rotationBegin(), which starts the Makapix client
  liveBegin();
  MEMORY_AT("reporter");
  telemetryBootMark("reporter");
  modeBegin();
  MEMORY_AT("mode");
  telemetryBootMark("mode");
  transitionBegin();
  MEMORY_AT("setup done");
  telemetryBootMark("setup done");
  outgoing = (uint16_t *)ps_malloc(PANEL_RES_X * PANEL_RES_Y * sizeof(uint16_t));
  MakapixSlot::whileWaiting = serviceWhileWaiting;
}

void loop() {
  if (!display) {
    delay(1000);
    return;
  }

  MEMORY_CHECK("display loop");
  connectionMaintain();
  firmwareUpdateMaintain(apiRunning());
  if (firmwareUpdateActive()) {
    showUpdateProgress();
    return;
  }
  clockChipMaintain();
  nightTick();

  // Who has the panel this turn: screen_rules decides between the night
  // schedule, sound mode, a request and the rotation.
  bool nightOff = nightNow().off;
  screen::Source source =
      screen::choose(screenInputs(requestedPost != 0 || requestedUpload != 0 || requestedStarter != 0 || requestedTest >= 0));
  statusSetScreen(screen::name(source));
  if (source == screen::Source::Dark) {
    lastSource = source;
    nightDark();
    return;
  }
  if (source == screen::Source::Request && nightOff) {
    nightWake(); // a request lights a dark panel for a while
  }

  // Outside sound mode the microphones are switched off, which gives their
  // buffers back to internal RAM. A request shown in sound mode leaves them on.
  if (!modeIsSound() && audioRunning()) {
    audioEnd();
    Serial.println("Audio stopped");
  }
  // A requested artwork plays for one turn; then whatever was on carries on
  // where it was.
  bool fromBack = false;
  bool testing = false;
  if (source == screen::Source::Request) {
    if (requestedTest >= 0) {
      testSlot.use((testpattern::Pattern)requestedTest);
      testing = true;
    } else if (requestedUpload) {
      rotationArtworkSlot()->requestUpload(requestedUpload);
    } else if (requestedStarter) {
      rotationArtworkSlot()->requestStarter(requestedStarter);
    } else {
      rotationArtworkSlot()->request(requestedPost);
    }
    requestedTest = -1;
    requestedUpload = 0;
    requestedStarter = 0;
    requestedPost = 0;
    fromBack = requestFromBack;
    requestFromBack = false;
  }
  Slot *slot = testing                                ? &testSlot
               : source == screen::Source::Sound        ? modeSoundSlot()
               : source == screen::Source::Request    ? rotationArtworkSlot()
               : source == screen::Source::NightClock ? rotationClockSlot()
                                                      : rotationCurrent();

  // For a transition, the slot draws its first frame off the panel while the
  // old picture stays up, then the two are blended. Sound mode switches at
  // once, and the night clock does not blend into itself every ten seconds.
  // A test pattern appears at once: it is there to be looked at, not admired.
  bool animate = outgoing && !testing && transitionEnabled() && screen::transitionAllowed(source, lastSource);
  lastSource = source;
  if (animate) {
    memcpy(outgoing, display->pixels(), PANEL_RES_X * PANEL_RES_Y * sizeof(uint16_t));
    display->setLive(false);
  }
  bool started = slot->start();
  MEMORY_CHECK("slot start");
  if (animate) {
    if (started) {
      slot->update();
      transitionPlay(outgoing, display->pixels());
    }
    display->setLive(true);
  }

  if (started) {
    static bool firstShown = false;
    if (!firstShown) {
      firstShown = true;
      telemetryBootMark("first slot on screen");
    }
    slot->shown();
    skippedInARow = 0;
    statusSetSlot(slot->name());
    Serial.printf("Slot: %s\n", slot->name());
    PlayerStatus now = statusGet();
    if (now.hasArtwork && !fromBack) {
      history.shown(now.postId);
    }
    runSlot(slot, source);
    statusSetSlot("");
  } else if (source == screen::Source::Request) {
    // Not on the card and not available from Makapix: carry on as before.
  } else if (source == screen::Source::Sound) {
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
  if (screen::advancesRotation(source)) {
    rotationAdvance();
  }
}
