#pragma once

#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <string>

// View reports waiting to go to Makapix, kept apart from storage and the
// network so they can be tested. Views played offline wait here, and on the
// card as one line each, until the connection returns:
//
//   V <postId> <unixSeconds> <a|c>    a = asked for (gallery), c = the rotation's pick
//
// `unixSeconds` is 0 when the clock was not set; Makapix then records the view
// at the time it arrives.

namespace views {

struct View {
  uint32_t postId;
  uint32_t unixSeconds;
  bool requested; // asked for by the user rather than picked by the rotation
};

class Queue {
public:
  static const size_t CAPACITY = 1000;

  // Adds a view, dropping the oldest when full. Returns false if one was dropped.
  bool push(const View &view);
  bool empty() const { return items.empty(); }
  size_t size() const { return items.size(); }
  const View &front() const { return items.front(); }
  const View &at(size_t i) const { return items[i]; }
  void pop() { items.pop_front(); }
  void clear() { items.clear(); }

private:
  std::deque<View> items;
};

std::string toLine(const View &view);
// False, and `view` untouched, for a line that is malformed or cut short.
bool fromLine(const char *line, View &view);

// "2026-10-16T14:05:00Z"; 0 gives "1970-01-01T00:00:00Z".
std::string isoTime(uint32_t unixSeconds);

} // namespace views
