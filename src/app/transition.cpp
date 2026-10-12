#include "app/transition.h"

#include <Preferences.h>

#include "app/commands.h"
#include "display/display.h"
#include "transitions.h"

static const uint32_t DURATION_MS = 700;
static const uint32_t FRAME_MS = 25;
static const int RANDOM = -1;
static const int NONE = -2;

static Preferences prefs;
static SemaphoreHandle_t lock;
static int choice = RANDOM;   // RANDOM, NONE or an index into the transitions
static int lastPlayed = -1;
static uint16_t *frame = nullptr;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static int parse(const String &name) {
  if (name == "random") {
    return RANDOM;
  }
  if (name == "none") {
    return NONE;
  }
  int index = transitions::find(name.c_str());
  return index >= 0 ? index : -3;
}

void transitionBegin() {
  lock = xSemaphoreCreateMutex();
  frame = (uint16_t *)ps_malloc(transitions::PIXELS * sizeof(uint16_t));
  prefs.begin("jukebox", false);
  if (prefs.isKey("transition")) {
    int saved = parse(prefs.getString("transition", ""));
    if (saved >= NONE) {
      choice = saved;
    }
  }
}

String transitionChoice() {
  Guard guard;
  return choice == RANDOM ? "random" : choice == NONE ? "none" : transitions::info(choice).name;
}

bool transitionSetChoice(const String &name, String &error) {
  int parsed = parse(name);
  if (parsed < NONE) {
    error = "unknown transition: " + name;
    return false;
  }
  Guard guard;
  choice = parsed;
  prefs.putString("transition", name);
  return true;
}

bool transitionEnabled() {
  Guard guard;
  return choice != NONE && frame != nullptr;
}

// A Next press during a transition means "get on with it": finish at once and
// do not also skip the slot that is arriving. Other commands wait their turn.
static bool cutShort() {
  Command command;
  if (commandPeek(command) && command.type == CommandType::Next) {
    commandReceive(command);
    return true;
  }
  return false;
}

void transitionPlay(const uint16_t *from, const uint16_t *to) {
  int index;
  {
    Guard guard;
    index = choice;
  }
  if (index == NONE || !frame) {
    display->present(to);
    return;
  }
  if (index == RANDOM) {
    do {
      index = esp_random() % transitions::count();
    } while (index == lastPlayed && transitions::count() > 1);
  }
  lastPlayed = index;
  uint32_t seed = esp_random();

  uint32_t start = millis();
  for (;;) {
    uint32_t elapsed = millis() - start;
    if (elapsed >= DURATION_MS || cutShort()) {
      break;
    }
    transitions::render(index, (float)elapsed / DURATION_MS, seed, from, to, frame);
    display->present(frame);
    uint32_t spent = millis() - start - elapsed;
    if (spent < FRAME_MS) {
      delay(FRAME_MS - spent);
    }
  }
  display->present(to);
}
