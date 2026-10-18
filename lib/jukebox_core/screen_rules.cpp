#include "screen_rules.h"

namespace screen {

Source choose(const Inputs &inputs) {
  if (inputs.requestPending) {
    return Source::Request;
  }
  if (inputs.nightOff) {
    return Source::Dark;
  }
  if (inputs.sound) {
    return Source::Sound;
  }
  if (inputs.nightClock) {
    return Source::NightClock;
  }
  return Source::Rotation;
}

const char *name(Source source) {
  switch (source) {
    case Source::Dark:
      return "dark";
    case Source::Request:
      return "request";
    case Source::Sound:
      return "sound";
    case Source::NightClock:
      return "nightClock";
    default:
      return "rotation";
  }
}

bool shouldGiveWay(Source running, const Inputs &now) {
  if (running == Source::Request) {
    return false;
  }
  Inputs withoutRequest = now;
  withoutRequest.requestPending = false;
  return choose(withoutRequest) != running;
}

bool advancesRotation(Source source) { return source == Source::Rotation; }

bool holdAllowed(Source source) { return source != Source::Sound && source != Source::Dark; }

bool nextChangesVisualizer(Source source) { return source == Source::Sound; }

bool transitionAllowed(Source source, Source previous) {
  if (source == Source::Sound || source == Source::Dark) {
    return false;
  }
  return !(source == Source::NightClock && previous == Source::NightClock);
}

void History::shown(uint32_t postId) {
  if (postId == 0 || (count_ > 0 && ids_[count_ - 1] == postId)) {
    return;
  }
  if (count_ == MAX) {
    for (size_t i = 1; i < MAX; i++) {
      ids_[i - 1] = ids_[i];
    }
    count_--;
  }
  ids_[count_++] = postId;
}

uint32_t History::back() {
  if (count_ < 2) {
    return 0;
  }
  count_--; // drop the current one
  return ids_[count_ - 1];
}

} // namespace screen
