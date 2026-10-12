#include "app/mode.h"

#include <Preferences.h>

#include "app/commands.h"
#include "slots/slot_visualizers.h"

static SpectrumSlot spectrumSlot;
static VuSlot vuSlot;
static ScopeSlot scopeSlot;
static FireSlot fireSlot;

struct Visualizer {
  SlotInfo info;
  Slot *slot;
};
static const Visualizer VISUALIZERS[] = {
    {{"spectrum", "Spectrum bars", ""}, &spectrumSlot},
    {{"vu", "VU meter", ""}, &vuSlot},
    {{"scope", "Oscilloscope", ""}, &scopeSlot},
    {{"fire", "Fire", ""}, &fireSlot},
};
static const size_t VISUALIZER_COUNT = sizeof(VISUALIZERS) / sizeof(VISUALIZERS[0]);

static Preferences prefs;
static SemaphoreHandle_t lock;
static bool sound = false;
static size_t selected = 0;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static int findVisualizer(const String &name) {
  for (size_t i = 0; i < VISUALIZER_COUNT; i++) {
    if (name == VISUALIZERS[i].info.name) {
      return i;
    }
  }
  return -1;
}

void modeBegin() {
  lock = xSemaphoreCreateMutex();
  prefs.begin("jukebox", false);
  if (prefs.isKey("visualizer")) {
    int saved = findVisualizer(prefs.getString("visualizer", ""));
    if (saved >= 0) {
      selected = saved;
    }
  }
}

const std::vector<SlotInfo> &modeVisualizers() {
  static const std::vector<SlotInfo> list = [] {
    std::vector<SlotInfo> items;
    for (const Visualizer &visualizer : VISUALIZERS) {
      items.push_back(visualizer.info);
    }
    return items;
  }();
  return list;
}

bool modeIsSound() {
  Guard guard;
  return sound;
}

String modeVisualizer() {
  Guard guard;
  return VISUALIZERS[selected].info.name;
}

bool modeSet(bool wantSound, const String &visualizer, String &error) {
  bool changed = false;
  {
    Guard guard;
    if (wantSound && !visualizer.isEmpty()) {
      int index = findVisualizer(visualizer);
      if (index < 0) {
        error = "unknown visualizer: " + visualizer;
        return false;
      }
      if ((size_t)index != selected) {
        selected = index;
        prefs.putString("visualizer", VISUALIZERS[selected].info.name);
        changed = true;
      }
    }
    changed = changed || sound != wantSound;
    sound = wantSound;
  }
  if (changed) {
    commandSend(CommandType::Reload); // ends whatever is on screen
  }
  return true;
}

Slot *modeSoundSlot() {
  Guard guard;
  return VISUALIZERS[selected].slot;
}

void modeNextVisualizer() {
  Guard guard;
  selected = (selected + 1) % VISUALIZER_COUNT;
  prefs.putString("visualizer", VISUALIZERS[selected].info.name);
}
