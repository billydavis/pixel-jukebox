#pragma once

// Tap detection on accelerometer samples, kept apart from the sensor so it can
// be tested.
//
// Adapted from p64 (https://github.com/fabkury/p64),
// firmware/components/p64_inputs/src/tap.hpp and tap.cpp.
// Copyright 2026 Fabrício Kury. Licensed under the Apache License 2.0.
// Changes: renamed to this project's style and reports the peak of each tap.
//
// A tap is a short impulse: the acceleration, with gravity taken out, crosses a
// threshold and falls back within a few tens of milliseconds. A longer
// excursion is the panel being moved, not tapped. Two impulses close together
// make a double tap; a lone impulse is reported once the window for a second
// one has closed; a one-second lockout follows every report.

#include <stdint.h>

namespace tap {

enum class Gesture : uint8_t { None, Single, Double };

class Detector {
public:
  static const uint32_t IMPULSE_MAX_MS = 60;    // longer than this is motion, not a tap
  static const uint32_t DOUBLE_MIN_MS = 80;     // a second impulse sooner is the same knock ringing
  static const uint32_t DOUBLE_WINDOW_MS = 350; // and later is a new tap
  static const uint32_t LOCKOUT_MS = 1000;

  // Sensitivity 1 (firm knock, 2.5 g) to 10 (light touch, 0.25 g); 5 is 1.5 g.
  static float thresholdFor(int sensitivity);
  void setSensitivity(int sensitivity);
  float threshold() const { return threshold_; }

  // One sample in g, `ms` a millisecond clock. Returns the gesture decided at
  // this sample, if any.
  Gesture feed(uint32_t ms, float ax, float ay, float az);

  // The largest impulse (g above gravity) seen since the last call.
  float takePeak();
  // The largest impulse of the tap or taps just reported.
  float lastTapPeak() const { return lastTapPeak_; }
  void reset();

private:
  float threshold_ = 1.5f;
  float gravity_ = 0;      // the magnitude, low-passed
  bool settled_ = false;   // gravity_ has a first value
  bool above_ = false;     // inside an excursion
  uint32_t aboveSince_ = 0;
  float excursionPeak_ = 0;
  bool pending_ = false;   // a first impulse waiting for a second
  uint32_t firstTapMs_ = 0;
  float pendingPeak_ = 0;
  uint32_t lockoutUntil_ = 0;
  uint32_t lastMs_ = 0;
  float peak_ = 0;
  float lastTapPeak_ = 0;
};

} // namespace tap
