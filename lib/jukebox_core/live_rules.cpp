#include "live_rules.h"

namespace live {

uint32_t retryDelaySeconds(int failures) {
  if (failures <= 0) {
    return 1;
  }
  uint32_t delay = 1;
  for (int i = 1; i < failures && delay < 300; i++) {
    delay *= 2;
  }
  return delay > 300 ? 300 : delay;
}

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's
// days_from_civil).
static long daysFromCivil(int year, int month, int day) {
  year -= month <= 2;
  const long era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = (unsigned)(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + (long)dayOfEra - 719468;
}

long daysUntil(int year, int month, int day, int64_t nowUnix) {
  int64_t nowDays = nowUnix >= 0 ? nowUnix / 86400 : -((-nowUnix + 86399) / 86400);
  return daysFromCivil(year, month, day) - (long)nowDays;
}

bool shouldRenew(long daysLeft, bool daysKnown, int handshakeFailures, bool renewedToday) {
  if (renewedToday) {
    return false;
  }
  return (daysKnown && daysLeft <= RENEW_WITHIN_DAYS) || handshakeFailures >= RENEW_AFTER_FAILURES;
}

} // namespace live
