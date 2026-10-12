#pragma once

#include "network/makapix.h"
#include "slots/slot.h"

// Plays one Makapix artwork per turn, preloading the next in the background.
// While the device is not registered it shows the registration code instead.
class MakapixSlot : public Slot {
 public:
  void begin();

  const char *name() const override { return "makapix"; }
  bool start() override;
  void shown() override;
  uint32_t update() override;
  bool finished() const override;
  void redraw() override;
  void stop() override;

 private:
  enum class Mode { Idle, Registering, Playing };

  bool startRegistration();
  bool startPlaying();
  uint32_t updateRegistration();
  uint32_t updatePlaying();

  Mode mode = Mode::Idle;
  uint32_t startedAt = 0;

  // Registration
  String code;
  uint32_t codeIssuedAt = 0;
  uint32_t lastPollAt = 0;
  uint32_t pollIntervalMs = 0;
  bool linked = false;
  uint32_t linkedAt = 0;

  // Playback
  uint8_t *data = nullptr;
  uint32_t dwellMs = 0;
  int framesThisLoop = 0;
  bool loopEnded = false;
  bool still = false;
  bool failed = false;
};
