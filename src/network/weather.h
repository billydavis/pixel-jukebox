#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// Current conditions from WeatherAPI.com (backlog #12), for the weather slot.
// Fetched over HTTPS by the reporter task, every 15 to 60 minutes, and only
// while a weather slot is in the rotation and the panel is not off for the
// night. A reading is kept for at most 60 minutes, as WeatherAPI.com's terms
// allow (weather_rules.h). Every call is safe from any task.
//
// The key: one entered in Settings, or else WEATHERAPI_KEY from include/secrets.h
// at build time. It is never sent back out through the API.

struct WeatherPlace {
  String name;    // "Georgetown"
  String region;  // "Texas"
  String country; // "United States of America"
  float lat = 0;
  float lon = 0;
  bool set() const { return !name.isEmpty(); }
};

struct WeatherReading {
  bool valid = false;
  float tempC = 0;
  float tempF = 0;
  int code = 0;   // WeatherAPI.com condition code
  bool isDay = true;
  String text;    // "Partly cloudy"
  String tzId;    // the town's time zone, "America/Chicago"
  uint32_t fetchedAt = 0; // millis() when it arrived
};

void weatherBegin();

// From the reporter task, about once a second: fetches when due, and runs a
// town search the API is waiting on.
void weatherMaintain();

bool weatherKeySet();
WeatherPlace weatherPlace();
bool weatherFahrenheit();
int weatherRefreshMinutes();

// Saved at once. An empty key removes the one from Settings (the build's, if
// any, is used). A new key or place is fetched at once; units need no fetch,
// since every reading has both. weatherSetRefresh() is false with `error`, and
// changes nothing, outside 15 to 60 minutes.
void weatherSetKey(const String &key);
void weatherSetPlace(const WeatherPlace &place);
void weatherSetFahrenheit(bool fahrenheit);
bool weatherSetRefresh(int minutes, String &error);

// The latest reading, invalid when there is none or it is past 60 minutes.
WeatherReading weatherReading();
// Why there is no reading, for the control page ("no key", "the key was
// refused"...), or empty.
String weatherProblem();
// True while a weather slot is in the rotation, switched on.
bool weatherWanted();

// Towns matching `query`, from WeatherAPI.com's search, for Settings. Runs on
// the reporter task; waits up to 10 s. False with `error` on failure.
bool weatherSearch(const String &query, std::vector<WeatherPlace> &places, String &error);

void weatherReport(JsonObject out);
