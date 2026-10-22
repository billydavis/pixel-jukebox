#pragma once

#include <stdint.h>

// The clock's settings and the text and colors it shows (backlog #25), kept
// apart from the display so they can be tested.

namespace clockface {

enum class Style : uint8_t { Digital, Big, Analog, COUNT };
enum class DateFormat : uint8_t { Off, Weekday, DayMonth, Numeric, COUNT }; // "Thu Oct 22", "22 Oct", "10/22"
enum class Color : uint8_t { White, Amber, Mint, TimeOfDay, Custom, COUNT };

struct Rgb {
  uint8_t r, g, b;
};

struct Settings {
  Style style = Style::Digital;
  bool hours24 = false;
  bool seconds = false;
  DateFormat date = DateFormat::Weekday;
  Color color = Color::White;
  Rgb custom = {255, 128, 0}; // used when color is Custom
};

// Names as stored and sent: "digital", "big", "analog"; "off", "weekday",
// "daymonth", "numeric"; "white", "amber", "mint", "timeofday", "custom".
const char *styleName(Style style);
const char *dateName(DateFormat date);
const char *colorName(Color color);
bool styleFrom(const char *name, Style &style);
bool dateFrom(const char *name, DateFormat &date);
bool colorFrom(const char *name, Color &color);

// The custom color as "#rrggbb", and back (either case; the # is needed).
void formatHex(Rgb c, char *out); // `out` holds 8
bool parseHex(const char *text, Rgb &c);

// Packed into one number to store, and back; anything unreadable gives the
// defaults. The custom color is kept in a second number (packColor).
uint32_t pack(const Settings &settings);
Settings unpack(uint32_t value, uint32_t color = 0);
uint32_t packColor(Rgb c);

// "6:42" (12-hour) or "06:42" (24-hour); `hour` 0-23. `out` holds 6.
void formatTime(int hour, int minute, bool hours24, char *out);
// "AM" or "PM".
const char *meridiem(int hour);
// The date in `format`; `weekday` 0 = Sunday, `month` 1-12. Empty when Off. `out` holds 12.
void formatDate(DateFormat format, int weekday, int month, int day, char *out);

// The clock's main color at this time of day (hour 0-23, minute 0-59). Fixed
// colors ignore the time; TimeOfDay goes from a dim red at night through
// orange at dawn to white at midday, and back through amber in the evening;
// Custom is `custom`.
Rgb colorAt(Color color, Rgb custom, int hour, int minute);
// A dimmer shade of `c`, for the date, the ticks and AM/PM.
Rgb dimmed(Rgb c);

} // namespace clockface
