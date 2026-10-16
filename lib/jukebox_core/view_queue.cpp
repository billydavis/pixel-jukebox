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
  char line[40];
  snprintf(line, sizeof(line), "V %lu %lu %c\n", (unsigned long)view.postId, (unsigned long)view.unixSeconds,
           view.requested ? 'a' : 'c');
  return line;
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
  if ((kind != 'a' && kind != 'c') || (end[2] != '\n' && end[2] != '\0' && end[2] != '\r')) {
    return false; // includes a line cut short before its end
  }
  view = {(uint32_t)postId, (uint32_t)seconds, kind == 'a'};
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
