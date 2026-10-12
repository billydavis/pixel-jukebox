#include "rotation.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "rotation_rules.h"
#include "slot_clock.h"
#include "slot_makapix.h"

static MakapixSlot makapixSlot;
static ClockSlot clockSlot;

struct SlotType {
  SlotInfo info;
  Slot *slot;
};
static const SlotType SLOT_TYPES[] = {
    {{"makapix", "Makapix artwork", "one artwork"}, &makapixSlot},
    {{"clock", "Clock", "10 seconds"}, &clockSlot},
};

static const char *PREFS_KEY = "rotation";

static Preferences prefs;
static SemaphoreHandle_t lock;
static std::vector<RotationEntry> rotation;
static size_t position = 0;
static uint8_t turnsTaken = 0;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static Slot *findSlot(const String &name) {
  for (const SlotType &type : SLOT_TYPES) {
    if (name == type.info.name) {
      return type.slot;
    }
  }
  return nullptr;
}

static bool validate(const std::vector<RotationEntry> &entries, String &error) {
  std::vector<RotationItem> items;
  for (const RotationEntry &entry : entries) {
    items.push_back({entry.slot.c_str(), entry.turns});
  }
  std::vector<std::string> known;
  for (const SlotType &type : SLOT_TYPES) {
    known.push_back(type.info.name);
  }
  std::string problem;
  if (rotationValid(items, known, ROTATION_MAX_ENTRIES, ROTATION_MAX_TURNS, problem)) {
    return true;
  }
  error = problem.c_str();
  return false;
}

static std::vector<RotationEntry> defaultRotation() { return {{"makapix", 3}, {"clock", 1}}; }

// Stored as JSON: [{"slot":"makapix","turns":3}, ...]
static void save(const std::vector<RotationEntry> &entries) {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (const RotationEntry &entry : entries) {
    JsonObject item = array.add<JsonObject>();
    item["slot"] = entry.slot;
    item["turns"] = entry.turns;
  }
  String json;
  serializeJson(doc, json);
  prefs.putString(PREFS_KEY, json);
}

static std::vector<RotationEntry> load() {
  JsonDocument doc;
  String json = prefs.getString(PREFS_KEY, "");
  if (json.isEmpty() || deserializeJson(doc, json)) {
    return defaultRotation();
  }
  std::vector<RotationEntry> entries;
  for (JsonObject item : doc.as<JsonArray>()) {
    entries.push_back({item["slot"] | "", (uint8_t)(item["turns"] | 0)});
  }
  String error;
  return validate(entries, error) ? entries : defaultRotation();
}

void rotationBegin() {
  lock = xSemaphoreCreateMutex();
  prefs.begin("jukebox", false);
  makapixSlot.begin();
  rotation = load();
}

const std::vector<SlotInfo> &rotationAvailable() {
  static const std::vector<SlotInfo> available = [] {
    std::vector<SlotInfo> list;
    for (const SlotType &type : SLOT_TYPES) {
      list.push_back(type.info);
    }
    return list;
  }();
  return available;
}

std::vector<RotationEntry> rotationGet() {
  Guard guard;
  return rotation;
}

bool rotationSet(const std::vector<RotationEntry> &entries, String &error) {
  if (!validate(entries, error)) {
    return false;
  }
  Guard guard;
  rotation = entries;
  // rotationAdvance() runs when the slot on screen ends; start it at the top.
  position = rotation.size() - 1;
  turnsTaken = rotation[position].turns;
  save(rotation);
  return true;
}

Slot *rotationCurrent() {
  Guard guard;
  return findSlot(rotation[position].slot);
}

void rotationAdvance() {
  Guard guard;
  if (++turnsTaken >= rotation[position].turns) {
    turnsTaken = 0;
    position = (position + 1) % rotation.size();
  }
}

size_t rotationTurns() {
  Guard guard;
  size_t turns = 0;
  for (const RotationEntry &entry : rotation) {
    turns += entry.turns;
  }
  return turns;
}
