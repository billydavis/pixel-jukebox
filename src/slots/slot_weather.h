#pragma once

#include "slots/slot.h"

// The current weather (network/weather.h): a pixel-art picture of the
// conditions and the temperature. Skipped until a key and a town are set in
// Settings; shows "no data" when there is no reading under 60 minutes old.
class WeatherSlot : public Slot {
 public:
  const char *name() const override { return "weather"; }
  bool start() override;
  void shown() override;
  uint32_t update() override;
  bool finished() const override;
  void redraw() override;
  void useSeconds(uint8_t seconds) override { showMs = seconds * 1000UL; }

 private:
  void draw();

  uint32_t startedAt = 0;
  uint32_t showMs = 10000;
  uint32_t drawnFetchedAt = 0; // the reading on screen, to redraw when a new one arrives
  int drawnAgeMinutes = -1;
};
