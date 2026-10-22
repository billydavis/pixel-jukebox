#include "slots/slot_weather.h"

#include "display/display.h"
#include "display/weather_icons.h"
#include "network/weather.h"
#include "pins.h"
#include "weather_rules.h"

static const uint32_t UPDATE_MS = 1000;
static const int ICON_X = (PANEL_RES_X - WEATHER_ICON_SIZE) / 2;
static const int ICON_Y = 3;

bool WeatherSlot::start() {
  if (!weatherKeySet() || !weatherPlace().set()) {
    return false; // not set up: nothing to show
  }
  startedAt = millis();
  draw();
  return true;
}

void WeatherSlot::shown() { startedAt = millis(); }

uint32_t WeatherSlot::update() {
  WeatherReading now = weatherReading();
  int ageMinutes = now.valid ? (int)((millis() - now.fetchedAt) / 60000) : -1;
  if ((now.valid ? now.fetchedAt : 0) != drawnFetchedAt || ageMinutes != drawnAgeMinutes) {
    draw();
  }
  return UPDATE_MS;
}

bool WeatherSlot::finished() const { return millis() - startedAt >= showMs; }

void WeatherSlot::redraw() { drawnAgeMinutes = -2; }

static void drawIcon(int index) {
  const uint16_t *pixels = WEATHER_ICONS[index];
  for (int y = 0; y < WEATHER_ICON_SIZE; y++) {
    for (int x = 0; x < WEATHER_ICON_SIZE; x++) {
      uint16_t colour = pixels[y * WEATHER_ICON_SIZE + x];
      if (colour) {
        display->drawPixel(ICON_X + x, ICON_Y + y, colour);
      }
    }
  }
}

// The temperature centred, with a small degree ring after it; `size` is the
// font scale (6 x 8 pixels per character at 1).
static void drawTemperature(int degrees, int y, uint8_t size, uint16_t colour) {
  char text[8];
  snprintf(text, sizeof(text), "%d", degrees);
  int textWidth = strlen(text) * 6 * size - size;
  int ring = size + 1; // the degree ring's radius
  int width = textWidth + size + 2 * ring + 1;
  int x = (PANEL_RES_X - width) / 2;
  display->setTextSize(size);
  display->setTextColor(colour);
  display->setCursor(x, y);
  display->print(text);
  display->drawCircle(x + textWidth + size + ring, y + ring, ring, colour);
}

static void printCentred(const char *text, int y, uint16_t colour) {
  int width = strlen(text) * 6 - 1;
  display->setTextSize(1);
  display->setTextColor(colour);
  display->setCursor((PANEL_RES_X - width) / 2, y);
  display->print(text);
}

void WeatherSlot::draw() {
  WeatherReading now = weatherReading();
  display->clearScreen();
  display->setTextWrap(false);
  uint16_t white = display->color565(255, 255, 255);
  uint16_t dim = display->color565(120, 120, 120);

  if (!now.valid) {
    drawnFetchedAt = 0;
    drawnAgeMinutes = -1;
    printCentred("Weather", 20, colour::amber());
    printCentred("no data", 34, dim);
    return;
  }

  uint32_t ageMs = millis() - now.fetchedAt;
  drawnFetchedAt = now.fetchedAt;
  drawnAgeMinutes = (int)(ageMs / 60000);

  weather::Icon icon = weather::iconFor(now.code);
  int index = (int)icon;
  if (!now.isDay && icon == weather::Icon::Clear) {
    index = WEATHER_ICON_NIGHT_CLEAR;
  } else if (!now.isDay && icon == weather::Icon::PartlyCloudy) {
    index = WEATHER_ICON_NIGHT_PARTLY_CLOUDY;
  }
  drawIcon(index);

  int degrees = weather::roundTemperature(weatherFahrenheit() ? now.tempF : now.tempC);
  bool aged = weather::freshness(ageMs, weatherRefreshMinutes()) == weather::Freshness::Aged;
  bool small = aged || degrees <= -10 || degrees >= 100; // three characters, or room for the age
  drawTemperature(degrees, 39, small ? 2 : 3, white);
  if (aged) {
    char age[12];
    snprintf(age, sizeof(age), "%dm old", drawnAgeMinutes);
    printCentred(age, 56, dim);
  }
}
