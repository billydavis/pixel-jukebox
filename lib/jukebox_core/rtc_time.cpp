#include "rtc_time.h"

namespace rtc {

static const uint32_t YEAR_2000 = 946684800;   // 2000-01-01T00:00:00Z
static const uint32_t YEAR_2100 = 4102444800u; // 2100-01-01T00:00:00Z

// Howard Hinnant's days_from_civil and civil_from_days.
int32_t daysFromCivil(int year, int month, int day) {
  year -= month <= 2 ? 1 : 0;
  int32_t era = (year >= 0 ? year : year - 399) / 400;
  int32_t yearOfEra = year - era * 400;
  int32_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + dayOfEra - 719468;
}

void civilFromDays(int32_t days, int &year, int &month, int &day) {
  days += 719468;
  int32_t era = (days >= 0 ? days : days - 146096) / 146097;
  int32_t dayOfEra = days - era * 146097;
  int32_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  int32_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  int32_t monthIndex = (5 * dayOfYear + 2) / 153;
  day = dayOfYear - (153 * monthIndex + 2) / 5 + 1;
  month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
  year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
}

static bool fromBcd(uint8_t value, int low, int high, int &out) {
  if ((value & 0x0F) > 9 || (value >> 4) > 9) {
    return false;
  }
  out = (value >> 4) * 10 + (value & 0x0F);
  return out >= low && out <= high;
}

static uint8_t toBcd(int value) { return (uint8_t)(((value / 10) << 4) | (value % 10)); }

static int daysInMonth(int year, int month) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  return month == 2 && leap ? 29 : days[month - 1];
}

bool decode(const uint8_t regs[REGISTERS], uint32_t &unixSeconds) {
  if (regs[0] & 0x80) {
    return false; // the oscillator stopped: the time was lost
  }
  int second, minute, hour, day, month, year;
  if (!fromBcd(regs[0] & 0x7F, 0, 59, second) || !fromBcd(regs[1] & 0x7F, 0, 59, minute) ||
      !fromBcd(regs[2] & 0x3F, 0, 23, hour) || !fromBcd(regs[3] & 0x3F, 1, 31, day) ||
      !fromBcd(regs[5] & 0x1F, 1, 12, month) || !fromBcd(regs[6], 0, 99, year)) {
    return false;
  }
  year += 2000;
  if (day > daysInMonth(year, month)) {
    return false;
  }
  unixSeconds = (uint32_t)daysFromCivil(year, month, day) * 86400u + hour * 3600 + minute * 60 + second;
  return true;
}

void encode(uint32_t unixSeconds, uint8_t regs[REGISTERS]) {
  if (unixSeconds < YEAR_2000) {
    unixSeconds = YEAR_2000;
  } else if (unixSeconds >= YEAR_2100) {
    unixSeconds = YEAR_2100 - 1;
  }
  int32_t days = (int32_t)(unixSeconds / 86400);
  uint32_t secondOfDay = unixSeconds % 86400;
  int year, month, day;
  civilFromDays(days, year, month, day);
  regs[0] = toBcd(secondOfDay % 60); // the oscillator-stopped flag clear
  regs[1] = toBcd(secondOfDay / 60 % 60);
  regs[2] = toBcd(secondOfDay / 3600);
  regs[3] = toBcd(day);
  regs[4] = (uint8_t)((days + 4) % 7); // 1970-01-01 was a Thursday
  regs[5] = toBcd(month);
  regs[6] = toBcd(year - 2000);
}

} // namespace rtc
