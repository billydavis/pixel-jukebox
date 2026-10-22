#include "clock_rules.h"

#include <stdio.h>
#include <string.h>

namespace clockface {

static const char *const STYLES[] = {"digital", "big", "analog"};
static const char *const DATES[] = {"off", "weekday", "daymonth", "numeric"};
static const char *const COLORS[] = {"white", "amber", "mint", "timeofday", "custom"};

const char *styleName(Style style) { return STYLES[(int)style < (int)Style::COUNT ? (int)style : 0]; }
const char *dateName(DateFormat date) { return DATES[(int)date < (int)DateFormat::COUNT ? (int)date : 0]; }
const char *colorName(Color color) { return COLORS[(int)color < (int)Color::COUNT ? (int)color : 0]; }

template <typename E> static bool lookup(const char *const *names, int count, const char *name, E &out) {
  for (int i = 0; name && i < count; i++) {
    if (strcmp(names[i], name) == 0) {
      out = (E)i;
      return true;
    }
  }
  return false;
}

bool styleFrom(const char *name, Style &style) { return lookup(STYLES, (int)Style::COUNT, name, style); }
bool dateFrom(const char *name, DateFormat &date) { return lookup(DATES, (int)DateFormat::COUNT, name, date); }
bool colorFrom(const char *name, Color &color) { return lookup(COLORS, (int)Color::COUNT, name, color); }

void formatHex(Rgb c, char *out) { snprintf(out, 8, "#%02x%02x%02x", c.r, c.g, c.b); }

static int hexDigit(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

bool parseHex(const char *text, Rgb &c) {
  if (!text || text[0] != '#' || strlen(text) != 7) {
    return false;
  }
  int v[6];
  for (int i = 0; i < 6; i++) {
    if ((v[i] = hexDigit(text[i + 1])) < 0) {
      return false;
    }
  }
  c = {(uint8_t)(v[0] * 16 + v[1]), (uint8_t)(v[2] * 16 + v[3]), (uint8_t)(v[4] * 16 + v[5])};
  return true;
}

// The custom color: bits 0-23 the color, 31 set.
uint32_t packColor(Rgb c) { return 0x80000000u | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }

// Bits 0-3 style, 4 hours24, 5 seconds, 8-11 date, 12-15 color, 31 set.
uint32_t pack(const Settings &s) {
  return 0x80000000u | (uint32_t)s.style | (s.hours24 ? 0x10u : 0) | (s.seconds ? 0x20u : 0) |
         ((uint32_t)s.date << 8) | ((uint32_t)s.color << 12);
}

Settings unpack(uint32_t value, uint32_t color) {
  Settings s;
  if (color & 0x80000000u) {
    s.custom = {(uint8_t)(color >> 16), (uint8_t)(color >> 8), (uint8_t)color};
  }
  if (!(value & 0x80000000u)) {
    return s;
  }
  int style = value & 0xF, date = (value >> 8) & 0xF, which = (value >> 12) & 0xF;
  if (style >= (int)Style::COUNT || date >= (int)DateFormat::COUNT || which >= (int)Color::COUNT) {
    return s;
  }
  s.style = (Style)style;
  s.hours24 = value & 0x10;
  s.seconds = value & 0x20;
  s.date = (DateFormat)date;
  s.color = (Color)which;
  return s;
}

void formatTime(int hour, int minute, bool hours24, char *out) {
  if (hours24) {
    snprintf(out, 6, "%02d:%02d", hour, minute);
    return;
  }
  int twelve = hour % 12;
  snprintf(out, 6, "%d:%02d", twelve == 0 ? 12 : twelve, minute);
}

const char *meridiem(int hour) { return hour < 12 ? "AM" : "PM"; }

void formatDate(DateFormat format, int weekday, int month, int day, char *out) {
  static const char *const DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char *dayName = DAYS[(weekday % 7 + 7) % 7];
  const char *monthName = MONTHS[(month - 1 + 12) % 12];
  switch (format) {
    case DateFormat::Weekday:
      snprintf(out, 12, "%s %s %d", dayName, monthName, day);
      break;
    case DateFormat::DayMonth:
      snprintf(out, 12, "%d %s", day, monthName);
      break;
    case DateFormat::Numeric:
      snprintf(out, 12, "%d/%d", month, day);
      break;
    default:
      out[0] = '\0';
  }
}

// The time-of-day colors through the day, in minutes since midnight.
struct Key {
  int minute;
  Rgb color;
};
static const Key DAY[] = {
    {0, {110, 18, 8}},      // night: a dim red that does not light the room
    {5 * 60, {110, 18, 8}},
    {6 * 60 + 30, {255, 120, 40}}, // dawn: orange
    {9 * 60, {255, 228, 196}},     // morning: warm white
    {12 * 60, {255, 255, 255}},    // midday: white
    {17 * 60, {255, 228, 190}},
    {19 * 60, {255, 150, 50}},  // evening: amber
    {21 * 60, {210, 70, 24}},
    {23 * 60, {110, 18, 8}},
    {24 * 60, {110, 18, 8}},
};

static uint8_t blend(uint8_t a, uint8_t b, int num, int den) { return (uint8_t)(a + (b - a) * num / den); }

Rgb colorAt(Color color, Rgb custom, int hour, int minute) {
  switch (color) {
    case Color::Amber:
      return {255, 176, 32};
    case Color::Mint:
      return {96, 236, 196};
    case Color::Custom:
      return custom;
    case Color::TimeOfDay: {
      int at = ((hour % 24) + 24) % 24 * 60 + minute;
      for (size_t i = 1; i < sizeof(DAY) / sizeof(DAY[0]); i++) {
        if (at <= DAY[i].minute) {
          const Key &a = DAY[i - 1], &b = DAY[i];
          int num = at - a.minute, den = b.minute - a.minute;
          return {blend(a.color.r, b.color.r, num, den), blend(a.color.g, b.color.g, num, den),
                  blend(a.color.b, b.color.b, num, den)};
        }
      }
      return DAY[0].color;
    }
    default:
      return {255, 255, 255};
  }
}

Rgb dimmed(Rgb c) { return {(uint8_t)(c.r * 9 / 20), (uint8_t)(c.g * 9 / 20), (uint8_t)(c.b * 9 / 20)}; }

} // namespace clockface
