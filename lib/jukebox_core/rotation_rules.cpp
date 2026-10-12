#include "rotation_rules.h"

#include <algorithm>

bool rotationValid(const std::vector<RotationItem> &items, const std::vector<std::string> &knownSlots,
                   size_t maxEntries, int maxTurns, std::string &error) {
  if (items.empty() || items.size() > maxEntries) {
    error = "rotation must have 1 to " + std::to_string(maxEntries) + " entries";
    return false;
  }
  for (const RotationItem &item : items) {
    if (std::find(knownSlots.begin(), knownSlots.end(), item.slot) == knownSlots.end()) {
      error = "unknown slot: " + item.slot;
      return false;
    }
    if (item.turns < 1 || item.turns > maxTurns) {
      error = "turns must be 1 to " + std::to_string(maxTurns);
      return false;
    }
  }
  return true;
}
