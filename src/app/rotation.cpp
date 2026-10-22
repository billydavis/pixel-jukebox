#include "app/rotation.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "channels.h"
#include "rotation_rules.h"
#include "slots/slot_clock.h"
#include "slots/slot_makapix.h"
#include "slots/slot_weather.h"

static MakapixSlot makapixSlot;

MakapixSlot *rotationArtworkSlot() { return &makapixSlot; }
static ClockSlot clockSlot;

Slot *rotationClockSlot() {
  clockSlot.useSeconds(ROTATION_SECONDS_DEFAULT);
  return &clockSlot;
}
static WeatherSlot weatherSlot;

struct SlotType {
  SlotInfo info;
  Slot *slot;
};
static const SlotType SLOT_TYPES[] = {
    {{"makapix", "Makapix artwork", "one artwork", false}, &makapixSlot},
    {{"clock", "Clock", "a set time", true}, &clockSlot},
    {{"weather", "Weather", "a set time", true}, &weatherSlot},
};

static const char *PREFS_KEY = "rotation";

static Preferences prefs;
static SemaphoreHandle_t lock;
static std::vector<RotationEntry> rotation;
static size_t position = 0;
static uint8_t turnsTaken = 0;
static String channelOverride; // empty: each slot plays its own channel
static String overrideFrom;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static const SlotType *findType(const String &name) {
  for (const SlotType &type : SLOT_TYPES) {
    if (name == type.info.name) {
      return &type;
    }
  }
  return nullptr;
}

// A timed slot's seconds: missing (0) becomes the default; others have none.
static bool tidySeconds(std::vector<RotationEntry> &entries, String &error) {
  for (RotationEntry &entry : entries) {
    const SlotType *type = findType(entry.slot);
    if (!type || !type->info.timed) {
      if (entry.seconds) {
        error = "only the clock and the weather have seconds";
        return false;
      }
      continue;
    }
    if (entry.seconds == 0) {
      entry.seconds = ROTATION_SECONDS_DEFAULT;
    }
    if (entry.seconds < ROTATION_SECONDS_MIN || entry.seconds > ROTATION_SECONDS_MAX) {
      error = "seconds must be " + String(ROTATION_SECONDS_MIN) + " to " + String(ROTATION_SECONDS_MAX);
      return false;
    }
  }
  return true;
}

static Slot *findSlot(const String &name) {
  for (const SlotType &type : SLOT_TYPES) {
    if (name == type.info.name) {
      return type.slot;
    }
  }
  return nullptr;
}

// Checks each artwork slot's channel and writes it in canonical form; other
// slots may not have one.
static bool tidyChannels(std::vector<RotationEntry> &entries, String &error) {
  for (RotationEntry &entry : entries) {
    if (entry.slot != "makapix") {
      if (!entry.channel.isEmpty()) {
        error = "only Makapix artwork slots have a channel";
        return false;
      }
      continue;
    }
    channels::Channel channel;
    std::string problem;
    if (!channels::parse(entry.channel.c_str(), channel, problem)) {
      error = problem.c_str();
      return false;
    }
    entry.channel = channels::format(channel).c_str();
  }
  return true;
}

static bool validate(const std::vector<RotationEntry> &entries, String &error) {
  std::vector<RotationItem> items;
  for (const RotationEntry &entry : entries) {
    items.push_back({entry.slot.c_str(), entry.turns, entry.enabled});
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

static std::vector<RotationEntry> defaultRotation() {
  return {{"makapix", 3, "all", true, 0}, {"clock", 1, "", true, ROTATION_SECONDS_DEFAULT}};
}

// Stored as JSON: [{"slot":"makapix","turns":3,"channel":"promoted"}, ...],
// with "enabled":false on paused entries only.
static void save(const std::vector<RotationEntry> &entries) {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (const RotationEntry &entry : entries) {
    JsonObject item = array.add<JsonObject>();
    item["slot"] = entry.slot;
    item["turns"] = entry.turns;
    if (!entry.channel.isEmpty()) {
      item["channel"] = entry.channel;
    }
    if (!entry.enabled) {
      item["enabled"] = false;
    }
    if (entry.seconds) {
      item["seconds"] = entry.seconds;
    }
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
    entries.push_back({item["slot"] | "", (uint8_t)(item["turns"] | 0), item["channel"] | "", item["enabled"] | true,
                       (uint8_t)(item["seconds"] | 0)});
  }
  String error;
  return tidyChannels(entries, error) && validate(entries, error) && tidySeconds(entries, error) ? entries
                                                                                                : defaultRotation();
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

bool rotationSet(const std::vector<RotationEntry> &requested, String &error) {
  std::vector<RotationEntry> entries = requested;
  if (!validate(entries, error) || !tidyChannels(entries, error) || !tidySeconds(entries, error)) {
    return false;
  }
  Guard guard;
  rotation = entries;
  channelOverride = ""; // a rotation just chosen plays as chosen
  overrideFrom = "";
  // rotationAdvance() runs when the slot on screen ends; start it at the top.
  position = rotation.size() - 1;
  turnsTaken = rotation[position].turns;
  save(rotation);
  return true;
}

// Moves on from a paused entry to the next one that is on. There is always
// one: the rules refuse a rotation with every entry paused.
static void skipPaused() {
  for (size_t i = 0; i < rotation.size() && !rotation[position].enabled; i++) {
    position = (position + 1) % rotation.size();
    turnsTaken = 0;
  }
}

Slot *rotationCurrent() {
  Guard guard;
  skipPaused();
  Slot *slot = findSlot(rotation[position].slot);
  slot->useSeconds(rotation[position].seconds);
  if (slot == &makapixSlot) {
    makapixSlot.useChannel(channelOverride.isEmpty() ? rotation[position].channel : channelOverride);
  }
  return slot;
}

bool rotationSetChannelOverride(const String &spec, const char *from, String &error) {
  String canonical;
  if (!spec.isEmpty()) {
    channels::Channel channel;
    std::string problem;
    if (!channels::parse(spec.c_str(), channel, problem)) {
      error = problem.c_str();
      return false;
    }
    canonical = channels::format(channel).c_str();
  }
  Guard guard;
  channelOverride = canonical;
  overrideFrom = canonical.isEmpty() ? "" : from;
  return true;
}

String rotationChannelOverride(String *from) {
  Guard guard;
  if (from) {
    *from = overrideFrom;
  }
  return channelOverride;
}

void rotationAdvance() {
  Guard guard;
  if (++turnsTaken >= rotation[position].turns) {
    turnsTaken = 0;
    position = (position + 1) % rotation.size();
  }
  skipPaused();
}

size_t rotationTurns() {
  Guard guard;
  size_t turns = 0;
  for (const RotationEntry &entry : rotation) {
    turns += entry.enabled ? entry.turns : 0;
  }
  return turns;
}
