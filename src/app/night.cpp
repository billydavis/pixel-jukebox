#include "app/night.h"

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include "display/display.h"

static const uint32_t OVERRIDE_MAX_MS = 60UL * 60 * 1000;
static const uint32_t TICK_MS = 250;
static const time_t TRUSTED_AFTER = 1700000000; // before 2023, the clock was never set

static Preferences prefs;
static SemaphoreHandle_t lock;
static night::Settings settings;
static bool overrideOn = false;
static uint8_t overrideLevel = 0;
static uint32_t overrideUntil = 0; // millis()
static uint32_t lastTick = 0;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static bool localSecond(uint32_t &second, time_t &now) {
  now = time(nullptr);
  if (now < TRUSTED_AFTER) {
    return false;
  }
  struct tm local;
  localtime_r(&now, &local);
  second = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  return true;
}

void nightBegin() {
  lock = xSemaphoreCreateMutex();
  prefs.begin("night", false);
  night::Settings loaded;
  loaded.enabled = prefs.getBool("enabled", loaded.enabled);
  loaded.start = prefs.getUShort("start", loaded.start);
  loaded.end = prefs.getUShort("end", loaded.end);
  loaded.action = (night::Action)prefs.getUChar("action", (uint8_t)loaded.action);
  loaded.brightness = prefs.getUChar("level", loaded.brightness);
  loaded.fadeMinutes = prefs.getUChar("fade", loaded.fadeMinutes);
  std::string error;
  if (night::valid(loaded, error) && (uint8_t)loaded.action <= (uint8_t)night::Action::Clock) {
    settings = loaded;
  }
  Serial.printf("Night: %s, %s to %s, %s\n", settings.enabled ? "on" : "off", night::formatTime(settings.start).c_str(),
                night::formatTime(settings.end).c_str(), night::actionName(settings.action));
}

night::Settings nightSettings() {
  Guard guard;
  return settings;
}

bool nightSetSettings(const night::Settings &wanted, String &error) {
  std::string problem;
  if (!night::valid(wanted, problem)) {
    error = problem.c_str();
    return false;
  }
  Guard guard;
  settings = wanted;
  overrideOn = false;
  prefs.putBool("enabled", settings.enabled);
  prefs.putUShort("start", settings.start);
  prefs.putUShort("end", settings.end);
  prefs.putUChar("action", (uint8_t)settings.action);
  prefs.putUChar("level", settings.brightness);
  prefs.putUChar("fade", settings.fadeMinutes);
  return true;
}

// Called with the lock held.
static NightNow evaluate() {
  NightNow now;
  now.enabled = settings.enabled;
  uint8_t day = displayBrightness();
  now.brightness = day;
  uint32_t second;
  time_t clock;
  now.timeKnown = localSecond(second, clock);
  if (overrideOn && (int32_t)(millis() - overrideUntil) >= 0) {
    overrideOn = false;
  }
  if (!settings.enabled || !now.timeKnown) {
    return now;
  }
  now.night = night::isNight(settings, second);
  now.secondsToSwitch = night::secondsToNextSwitch(settings, second);
  if (overrideOn) {
    now.overridden = true;
    now.brightness = overrideLevel;
    return now;
  }
  float depth = night::depth(settings, second);
  uint8_t target = settings.action == night::Action::Off ? 0 : settings.brightness;
  now.brightness = night::blend(day, target, depth);
  now.off = now.night && settings.action == night::Action::Off && depth >= 1.0f;
  now.clockOnly = now.night && settings.action == night::Action::Clock;
  return now;
}

NightNow nightNow() {
  Guard guard;
  return evaluate();
}

void nightTick() {
  if (millis() - lastTick < TICK_MS) {
    return;
  }
  lastTick = millis();
  uint8_t level = nightNow().brightness;
  if (level != displayAppliedBrightness()) {
    displayApplyBrightness(level);
  }
}

// Called with the lock held.
static void startOverride(uint8_t level, uint32_t secondsToSwitch) {
  overrideOn = true;
  overrideLevel = level;
  uint32_t ms = OVERRIDE_MAX_MS;
  if (secondsToSwitch && secondsToSwitch * 1000UL < ms) {
    ms = secondsToSwitch * 1000UL;
  }
  overrideUntil = millis() + ms;
}

void nightSetBrightness(uint8_t level) {
  bool scheduled;
  {
    Guard guard;
    NightNow now = evaluate();
    // While the schedule is changing the brightness, a hand-picked level
    // overrides it; otherwise it is the day level.
    scheduled = now.overridden || now.brightness != displayBrightness() || now.night;
    if (scheduled) {
      startOverride(level, now.secondsToSwitch);
    }
  }
  if (!scheduled) {
    displaySetBrightness(level);
  }
  lastTick = 0;
  nightTick();
}

void nightWake() {
  {
    Guard guard;
    NightNow now = evaluate();
    if (!now.off && !now.clockOnly) {
      return;
    }
    startOverride(displayBrightness(), now.secondsToSwitch);
  }
  Serial.println("Night: woken for a while");
  lastTick = 0;
  nightTick();
}

void nightReport(JsonObject report) {
  Guard guard;
  report["enabled"] = settings.enabled;
  report["start"] = night::formatTime(settings.start);
  report["end"] = night::formatTime(settings.end);
  report["action"] = night::actionName(settings.action);
  report["brightness"] = settings.brightness;
  report["fadeMinutes"] = settings.fadeMinutes;

  NightNow now = evaluate();
  JsonObject state = report["now"].to<JsonObject>();
  const char *phase = !settings.enabled  ? "disabled"
                      : !now.timeKnown   ? "no time"
                      : now.overridden   ? "overridden"
                      : now.night        ? "night"
                                         : "day";
  state["phase"] = phase;
  state["panel"] = now.off ? "off" : now.clockOnly ? "clock" : "playing";
  state["brightness"] = now.brightness;
  if (settings.enabled && now.timeKnown) {
    time_t at = time(nullptr) + now.secondsToSwitch;
    struct tm local;
    localtime_r(&at, &local);
    state["nextSwitch"] = night::formatTime(local.tm_hour * 60 + local.tm_min);
  } else {
    state["nextSwitch"] = nullptr;
  }
  if (now.overridden) {
    state["overrideMinutesLeft"] = (overrideUntil - millis()) / 60000 + 1;
  }
}
