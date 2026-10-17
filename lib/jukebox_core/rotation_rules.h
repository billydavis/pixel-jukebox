#pragma once

#include <stddef.h>
#include <string>
#include <vector>

// The rules a rotation must meet, kept apart from where it is stored and which
// slots exist so they can be tested on their own.

struct RotationItem {
  std::string slot;
  int turns;
  bool enabled; // false: paused, kept in place but skipped
};

// True if `items` has 1 to `maxEntries` entries, every slot is in `knownSlots`,
// every turn count is 1 to `maxTurns` and at least one entry is enabled.
// Otherwise `error` says what is wrong.
bool rotationValid(const std::vector<RotationItem> &items, const std::vector<std::string> &knownSlots,
                   size_t maxEntries, int maxTurns, std::string &error);
