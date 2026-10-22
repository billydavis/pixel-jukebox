#include "network/weather.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

#include "app/night.h"
#include "app/rotation.h"
#include "ca_certs.h"
#include "config.h"
#include "network/locked_tls.h"
#include "weather_rules.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WEATHERAPI_KEY
#define WEATHERAPI_KEY ""
#endif

static const char *API_URL = "https://api.weatherapi.com/v1/";
static const uint16_t HTTP_TIMEOUT_MS = 15000;
static const uint32_t SEARCH_WAIT_MS = 10000;
static const int MAX_RESULTS = 8;

static Preferences prefs;
static SemaphoreHandle_t lock;
static String settingsKey; // entered in Settings; the build's key is used when empty
static WeatherPlace place;
static bool fahrenheit = true;
static int refreshMinutes = weather::REFRESH_DEFAULT;
static bool changed = true; // fetch at once: the settings are new
static WeatherReading reading;
static uint32_t retryAt = 0;
static String problem;

// A town search, handed from the API's task to the reporter's and waited for.
static SemaphoreHandle_t searchDone;
static bool searchPending = false;
static String searchQuery;
static std::vector<WeatherPlace> searchResults;
static String searchError;
static bool searchOk = false;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static String keyInUse() { return settingsKey.isEmpty() ? String(WEATHERAPI_KEY) : settingsKey; }

static void savePlace() {
  JsonDocument doc;
  doc["name"] = place.name;
  doc["region"] = place.region;
  doc["country"] = place.country;
  doc["lat"] = place.lat;
  doc["lon"] = place.lon;
  String json;
  serializeJson(doc, json);
  prefs.putString("place", json);
}

void weatherBegin() {
  lock = xSemaphoreCreateMutex();
  searchDone = xSemaphoreCreateBinary();
  prefs.begin("weather", false);
  settingsKey = prefs.getString("key", "");
  fahrenheit = prefs.getBool("fahrenheit", true);
  refreshMinutes = prefs.getInt("refresh", weather::REFRESH_DEFAULT);
  if (!weather::refreshValid(refreshMinutes)) {
    refreshMinutes = weather::REFRESH_DEFAULT;
  }
  JsonDocument doc;
  if (!deserializeJson(doc, prefs.getString("place", ""))) {
    place.name = doc["name"] | "";
    place.region = doc["region"] | "";
    place.country = doc["country"] | "";
    place.lat = doc["lat"] | 0.0f;
    place.lon = doc["lon"] | 0.0f;
  }
}

// One GET to WeatherAPI.com: the HTTP status (negative when it could not be
// reached), with the body in `body`. A new connection each time: a fetch every
// 15 minutes or more is not worth keeping one open. Reporter task only, so the
// one connection is never shared.
static int request(const String &path, String &body) {
  static LockedTls tls; // TLS memory comes from PSRAM (makapix::begin() sets that up)
  static bool trusted = false;
  if (!trusted) {
    tls.setCACert(MAKAPIX_ROOT_CAS); // ISRG Root X1 covers api.weatherapi.com too
    trusted = true;
  }
  HTTPClient http;
  http.setUserAgent(USER_AGENT);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(tls, String(API_URL) + path)) {
    return -1;
  }
  int status = http.GET();
  if (status > 0) {
    body = http.getString();
  }
  http.end();
  return status;
}

// WeatherAPI.com's error, from a reply like {"error":{"code":2006,"message":"..."}}.
static String apiError(const String &body, int status) {
  JsonDocument doc;
  if (!deserializeJson(doc, body) && doc["error"]["message"].is<const char *>()) {
    int code = doc["error"]["code"] | 0;
    if (code == 2006) {
      return "the key was refused";
    }
    if (code == 2007) {
      return "this month's free calls are used up";
    }
    if (code == 2008) {
      return "the key has been switched off";
    }
    return doc["error"]["message"].as<String>();
  }
  return status > 0 ? "WeatherAPI.com answered " + String(status) : String("WeatherAPI.com could not be reached");
}

static String query(const WeatherPlace &where) {
  char q[40];
  snprintf(q, sizeof(q), "%.4f,%.4f", where.lat, where.lon);
  return q;
}

static void fetch() {
  String key;
  WeatherPlace where;
  {
    Guard guard;
    key = keyInUse();
    where = place;
    changed = false;
  }
  if (key.isEmpty() || !where.set()) {
    Guard guard;
    problem = key.isEmpty() ? "no WeatherAPI.com key" : "no town chosen";
    return;
  }

  String body;
  int status = request("current.json?key=" + key + "&q=" + query(where), body);

  JsonDocument filter;
  filter["current"]["temp_c"] = true;
  filter["current"]["temp_f"] = true;
  filter["current"]["is_day"] = true;
  filter["current"]["condition"]["code"] = true;
  filter["current"]["condition"]["text"] = true;
  filter["location"]["tz_id"] = true;
  JsonDocument doc;
  if (status == 200 && !deserializeJson(doc, body, DeserializationOption::Filter(filter)) &&
      doc["current"]["condition"]["code"].is<int>()) {
    WeatherReading fresh;
    fresh.valid = true;
    fresh.tempC = doc["current"]["temp_c"] | 0.0f;
    fresh.tempF = doc["current"]["temp_f"] | 0.0f;
    fresh.isDay = (doc["current"]["is_day"] | 1) == 1;
    fresh.code = doc["current"]["condition"]["code"];
    fresh.text = doc["current"]["condition"]["text"] | "";
    fresh.tzId = doc["location"]["tz_id"] | "";
    fresh.fetchedAt = millis();
    if (fresh.fetchedAt == 0) {
      fresh.fetchedAt = 1; // 0 means "none"
    }
    Guard guard;
    reading = fresh;
    retryAt = 0;
    problem = "";
    Serial.printf("Weather: %s, %.1f C, code %d\n", fresh.text.c_str(), fresh.tempC, fresh.code);
    return;
  }

  String why = status == 200 ? String("an answer the panel could not read") : apiError(body, status);
  bool refused = status == 401 || status == 403;
  Guard guard;
  problem = why;
  retryAt = millis() + (refused ? weather::RETRY_REFUSED_MS : weather::RETRY_MS);
  if (retryAt == 0) {
    retryAt = 1;
  }
  Serial.printf("Weather: fetch failed (%d): %s\n", status, why.c_str());
}

static void runSearch() {
  String key;
  String q;
  {
    Guard guard;
    key = keyInUse();
    q = searchQuery;
  }
  std::vector<WeatherPlace> found;
  String error;
  bool ok = false;
  if (key.isEmpty()) {
    error = "enter a WeatherAPI.com key first";
  } else {
    String encoded;
    for (char c : q) {
      if (isalnum((unsigned char)c) || c == '-' || c == '.' || c == '_') {
        encoded += c;
      } else {
        char hex[4];
        snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
        encoded += hex;
      }
    }
    String body;
    int status = request("search.json?key=" + key + "&q=" + encoded, body);
    JsonDocument doc;
    if (status == 200 && !deserializeJson(doc, body) && doc.is<JsonArray>()) {
      for (JsonObject item : doc.as<JsonArray>()) {
        if ((int)found.size() >= MAX_RESULTS) {
          break;
        }
        WeatherPlace where;
        where.name = item["name"] | "";
        where.region = item["region"] | "";
        where.country = item["country"] | "";
        where.lat = item["lat"] | 0.0f;
        where.lon = item["lon"] | 0.0f;
        if (where.set()) {
          found.push_back(where);
        }
      }
      ok = true;
    } else {
      error = apiError(body, status);
    }
  }
  {
    Guard guard;
    searchResults = found;
    searchError = error;
    searchOk = ok;
    searchPending = false;
  }
  xSemaphoreGive(searchDone);
}

bool weatherWanted() {
  for (const RotationEntry &entry : rotationGet()) {
    if (entry.slot == "weather" && entry.enabled) {
      return true;
    }
  }
  return false;
}

void weatherMaintain() {
  bool searching;
  {
    Guard guard;
    searching = searchPending;
  }
  if (searching) {
    if (WiFi.status() == WL_CONNECTED) {
      runSearch();
    } else {
      Guard guard;
      searchError = "the panel is not on Wi-Fi";
      searchOk = false;
      searchPending = false;
      xSemaphoreGive(searchDone);
    }
  }
  if (WiFi.status() != WL_CONNECTED || !weatherWanted() || nightNow().off) {
    return;
  }
  bool due;
  {
    Guard guard;
    due = weather::fetchDue(changed, millis(), reading.valid ? reading.fetchedAt : 0, retryAt, refreshMinutes);
  }
  if (due) {
    fetch();
  }
}

bool weatherKeySet() {
  Guard guard;
  return !keyInUse().isEmpty();
}

WeatherPlace weatherPlace() {
  Guard guard;
  return place;
}

bool weatherFahrenheit() {
  Guard guard;
  return fahrenheit;
}

int weatherRefreshMinutes() {
  Guard guard;
  return refreshMinutes;
}

void weatherSetKey(const String &key) {
  Guard guard;
  settingsKey = key;
  prefs.putString("key", key);
  changed = true; // try the new key at once
  retryAt = 0;
}

void weatherSetPlace(const WeatherPlace &where) {
  Guard guard;
  place = where;
  savePlace();
  changed = true;
  retryAt = 0;
  reading = WeatherReading(); // the old town's weather is not this town's
}

void weatherSetFahrenheit(bool value) {
  Guard guard;
  fahrenheit = value;
  prefs.putBool("fahrenheit", value); // both units come in every reading: no new fetch needed
}

bool weatherSetRefresh(int minutes, String &error) {
  if (!weather::refreshValid(minutes)) {
    error = "refreshMinutes must be " + String(weather::REFRESH_MIN) + " to " + String(weather::REFRESH_MAX);
    return false;
  }
  Guard guard;
  refreshMinutes = minutes;
  prefs.putInt("refresh", minutes);
  return true;
}

WeatherReading weatherReading() {
  Guard guard;
  if (reading.valid && weather::freshness(millis() - reading.fetchedAt, refreshMinutes) ==
                           weather::Freshness::Expired) {
    return WeatherReading();
  }
  return reading;
}

String weatherProblem() {
  Guard guard;
  if (keyInUse().isEmpty()) {
    return "no WeatherAPI.com key";
  }
  if (!place.set()) {
    return "no town chosen";
  }
  return problem;
}

bool weatherSearch(const String &q, std::vector<WeatherPlace> &places, String &error) {
  String trimmed = q;
  trimmed.trim();
  if (trimmed.length() < 2 || trimmed.length() > 60) {
    error = "type 2 to 60 letters to search";
    return false;
  }
  {
    Guard guard;
    if (searchPending) {
      error = "a search is already running";
      return false;
    }
    searchQuery = trimmed;
    searchPending = true;
  }
  xSemaphoreTake(searchDone, 0); // clear a late signal from a search that timed out
  if (xSemaphoreTake(searchDone, pdMS_TO_TICKS(SEARCH_WAIT_MS)) != pdTRUE) {
    error = "WeatherAPI.com took too long to answer";
    return false;
  }
  Guard guard;
  places = searchResults;
  error = searchError;
  return searchOk;
}

void weatherReport(JsonObject out) {
  WeatherReading now = weatherReading();
  Guard guard;
  out["keySet"] = !keyInUse().isEmpty();
  out["keySource"] = !settingsKey.isEmpty() ? "settings" : String(WEATHERAPI_KEY).isEmpty() ? "none" : "build";
  if (place.set()) {
    JsonObject where = out["place"].to<JsonObject>();
    where["name"] = place.name;
    where["region"] = place.region;
    where["country"] = place.country;
    where["lat"] = place.lat;
    where["lon"] = place.lon;
  } else {
    out["place"] = nullptr;
  }
  out["units"] = fahrenheit ? "f" : "c";
  out["refreshMinutes"] = refreshMinutes;
  out["refreshMin"] = weather::REFRESH_MIN;
  out["refreshMax"] = weather::REFRESH_MAX;
  if (now.valid) {
    JsonObject r = out["reading"].to<JsonObject>();
    r["tempC"] = now.tempC;
    r["tempF"] = now.tempF;
    r["condition"] = now.text;
    r["code"] = now.code;
    r["icon"] = weather::iconName(weather::iconFor(now.code));
    r["isDay"] = now.isDay;
    r["ageSeconds"] = (millis() - now.fetchedAt) / 1000;
    r["timezone"] = now.tzId; // the town's, to suggest it for the clock
  } else {
    out["reading"] = nullptr;
  }
  String why = keyInUse().isEmpty() ? String("no WeatherAPI.com key") : !place.set() ? String("no town chosen") : problem;
  if (why.isEmpty()) {
    out["problem"] = nullptr;
  } else {
    out["problem"] = why;
  }
  out["inRotation"] = weatherWanted();
  out["credit"] = "Weather by WeatherAPI.com";
}
