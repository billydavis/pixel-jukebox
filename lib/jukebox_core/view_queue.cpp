#include "view_queue.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

namespace views {

bool Queue::push(const View &view) {
  bool dropped = false;
  if (items.size() >= CAPACITY) {
    items.pop_front();
    dropped = true;
  }
  items.push_back(view);
  return !dropped;
}

std::string toLine(const View &view) {
  char head[40];
  snprintf(head, sizeof(head), "V %lu %lu %c ", (unsigned long)view.postId, (unsigned long)view.unixSeconds,
           view.requested ? 'a' : 'c');
  std::string line = head;
  line += view.channel.empty() ? "all" : view.channel;
  if (!view.hashtag.empty()) {
    line += " " + view.hashtag;
  }
  return line + "\n";
}

// One word up to a space or the end of the line. False if empty or too long.
static bool word(const char *&at, std::string &out, size_t limit) {
  const char *start = at;
  while (*at && *at != ' ' && *at != '\n' && *at != '\r') {
    at++;
  }
  size_t length = (size_t)(at - start);
  if (length == 0 || length > limit) {
    return false;
  }
  out.assign(start, length);
  return true;
}

bool fromLine(const char *line, View &view) {
  if (line[0] != 'V' || line[1] != ' ' || !isdigit((unsigned char)line[2])) {
    return false;
  }
  char *end;
  unsigned long postId = strtoul(line + 2, &end, 10);
  if (end == line + 2 || *end != ' ' || postId == 0 || postId > 0xFFFFFFFFUL) {
    return false;
  }
  const char *timeAt = end + 1;
  if (!isdigit((unsigned char)*timeAt)) {
    return false;
  }
  unsigned long seconds = strtoul(timeAt, &end, 10);
  if (end == timeAt || *end != ' ' || seconds > 0xFFFFFFFFUL) {
    return false;
  }
  char kind = end[1];
  if (kind != 'a' && kind != 'c') {
    return false;
  }
  View parsed = {(uint32_t)postId, (uint32_t)seconds, kind == 'a', "", ""};
  const char *at = end + 2;
  if (*at == ' ') {
    at++;
    if (!word(at, parsed.channel, 16)) {
      return false;
    }
    for (char c : parsed.channel) {
      if (!(c >= 'a' && c <= 'z') && c != '_') {
        return false;
      }
    }
    if (*at == ' ') {
      at++;
      if (!word(at, parsed.hashtag, 256)) {
        return false;
      }
    }
    if (parsed.channel == "all") {
      parsed.channel.clear();
    }
  }
  if (*at != '\n' && *at != '\0' && *at != '\r') {
    return false; // more than expected, or a word cut short
  }
  view = parsed;
  return true;
}

std::string isoTime(uint32_t unixSeconds) {
  // Days since 1970 to a calendar date (Howard Hinnant's civil_from_days).
  long days = (long)(unixSeconds / 86400);
  uint32_t secondOfDay = unixSeconds % 86400;
  days += 719468;
  long era = days / 146097;
  long dayOfEra = days - era * 146097;
  long yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  long dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  long monthIndex = (5 * dayOfYear + 2) / 153;
  long day = dayOfYear - (153 * monthIndex + 2) / 5 + 1;
  long month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
  long year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);

  char text[24];
  snprintf(text, sizeof(text), "%04ld-%02ld-%02ldT%02lu:%02lu:%02luZ", year, month, day,
           (unsigned long)(secondOfDay / 3600), (unsigned long)(secondOfDay / 60 % 60),
           (unsigned long)(secondOfDay % 60));
  return text;
}

} // namespace views
