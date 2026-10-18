#include "beat_follower.h"

// Per frame of about 32 ms: the average follows over roughly a second, and a
// beat's punch halves in about 100 ms.
static const float AVERAGE_RATE = 0.03f;
static const float PUNCH_FADE = 0.8f;
// A jump has to clear the average by this much to count, so noise and the
// natural wobble of a steady sound do not flicker.
static const float THRESHOLD = 0.06f;
// A jump of this much above the average is a full-strength beat.
static const float FULL_JUMP = 0.35f;

float BeatFollower::update(float level) {
  float jump = level - average_ - THRESHOLD;
  float hit = jump > 0 ? jump / FULL_JUMP : 0;
  if (hit > 1) {
    hit = 1;
  }
  punch_ *= PUNCH_FADE;
  if (hit > punch_) {
    punch_ = hit;
  }
  average_ += (level - average_) * AVERAGE_RATE;
  return punch_;
}

void BeatFollower::reset() {
  average_ = 0;
  punch_ = 0;
}
