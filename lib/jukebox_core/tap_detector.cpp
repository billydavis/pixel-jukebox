// Adapted from p64 (https://github.com/fabkury/p64),
// firmware/components/p64_inputs/src/tap.cpp.
// Copyright 2026 Fabrício Kury. Licensed under the Apache License 2.0.
// Changes: renamed to this project's style and reports the peak of each tap.

#include "tap_detector.h"

#include <cmath>

namespace tap {

float Detector::thresholdFor(int sensitivity) {
  if (sensitivity < 1) sensitivity = 1;
  if (sensitivity > 10) sensitivity = 10;
  return 2.75f - 0.25f * (float)sensitivity;
}

void Detector::setSensitivity(int sensitivity) { threshold_ = thresholdFor(sensitivity); }

void Detector::reset() {
  settled_ = false;
  above_ = false;
  pending_ = false;
  lockoutUntil_ = 0;
  peak_ = 0;
}

Gesture Detector::feed(uint32_t ms, float ax, float ay, float az) {
  const float magnitude = std::sqrt(ax * ax + ay * ay + az * az);
  if (!settled_) {
    gravity_ = magnitude;
    settled_ = true;
    lastMs_ = ms;
    return Gesture::None;
  }
  // Gravity is followed slowly (about 150 ms); a tap is what stands above it.
  const uint32_t dt = ms - lastMs_;
  lastMs_ = ms;
  const float alpha = dt >= 150 ? 1.0f : (float)dt / 150.0f;
  const float impulse = std::fabs(magnitude - gravity_);
  gravity_ += alpha * (magnitude - gravity_);
  if (impulse > peak_) peak_ = impulse;

  Gesture result = Gesture::None;
  const bool locked = (int32_t)(ms - lockoutUntil_) < 0;
  if (!above_) {
    if (impulse >= threshold_ && !locked) {
      above_ = true;
      aboveSince_ = ms;
      excursionPeak_ = impulse;
    }
  } else if (impulse < threshold_ * 0.5f) {
    above_ = false;
    if (ms - aboveSince_ <= IMPULSE_MAX_MS) {
      if (pending_) {
        const uint32_t gap = aboveSince_ - firstTapMs_;
        if (gap >= DOUBLE_MIN_MS && gap <= DOUBLE_WINDOW_MS) {
          pending_ = false;
          lockoutUntil_ = ms + LOCKOUT_MS;
          lastTapPeak_ = excursionPeak_ > pendingPeak_ ? excursionPeak_ : pendingPeak_;
          result = Gesture::Double;
        }
        // Sooner than DOUBLE_MIN_MS: the same knock ringing; ignore it.
      } else {
        pending_ = true;
        firstTapMs_ = aboveSince_;
        pendingPeak_ = excursionPeak_;
      }
    } else {
      pending_ = false; // a long excursion cancels a waiting tap: the panel was moved
    }
  } else {
    if (impulse > excursionPeak_) excursionPeak_ = impulse;
    if (ms - aboveSince_ > IMPULSE_MAX_MS) {
      // Still high after the impulse window: motion; give up on it.
      above_ = false;
      pending_ = false;
      lockoutUntil_ = ms + LOCKOUT_MS / 2;
    }
  }
  if (pending_ && !above_ && ms - firstTapMs_ > DOUBLE_WINDOW_MS) {
    pending_ = false;
    lockoutUntil_ = ms + LOCKOUT_MS;
    lastTapPeak_ = pendingPeak_;
    result = Gesture::Single;
  }
  return result;
}

float Detector::takePeak() {
  const float peak = peak_;
  peak_ = 0;
  return peak;
}

} // namespace tap
