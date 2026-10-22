#include "slots/slot_clock.h"

#include <Preferences.h>
#include <math.h>
#include <time.h>

#include "display/display.h"
#include "pins.h"

static const uint32_t UPDATE_MS = 200;

static Preferences prefs;
static portMUX_TYPE settingsLock = portMUX_INITIALIZER_UNLOCKED;
static clockface::Settings settings;

void clockBegin() {
  prefs.begin("clock", false);
  settings = clockface::unpack(prefs.getUInt("settings", 0), prefs.getUInt("color", 0));
}

clockface::Settings clockSettings() {
  portENTER_CRITICAL(&settingsLock);
  clockface::Settings copy = settings;
  portEXIT_CRITICAL(&settingsLock);
  return copy;
}

void clockSetSettings(const clockface::Settings &value) {
  portENTER_CRITICAL(&settingsLock);
  settings = value;
  portEXIT_CRITICAL(&settingsLock);
  prefs.putUInt("settings", clockface::pack(value));
  prefs.putUInt("color", clockface::packColor(value.custom));
}

bool ClockSlot::start() {
  struct tm now;
  if (!getLocalTime(&now, 0)) {
    return false; // not synchronised yet
  }
  startedAt = millis();
  draw(now);
  return true;
}

void ClockSlot::shown() { startedAt = millis(); }

uint32_t ClockSlot::update() {
  struct tm now;
  if (getLocalTime(&now, 0)) {
    clockface::Settings s = clockSettings();
    bool due = clockface::pack(s) != shownSettings || clockface::packColor(s.custom) != shownColor ||
               now.tm_min != shownMinute ||
               (s.seconds && now.tm_sec != shownSecond);
    if (due) {
      draw(now);
    }
  }
  return UPDATE_MS;
}

bool ClockSlot::finished() const { return millis() - startedAt >= showMs; }

void ClockSlot::redraw() { shownMinute = -1; }

// --- drawing -------------------------------------------------------------------------

static uint16_t rgb(clockface::Rgb c) { return display->color565(c.r, c.g, c.b); }

// Built-in font: 5 x 7 glyphs in 6 x 8 cells at size 1.
static int textWidth(const char *text, uint8_t size) { return (int)strlen(text) * 6 * size - size; }

static void printCentred(const char *text, int y, uint8_t size, uint16_t color) {
  display->setTextSize(size);
  display->setTextColor(color);
  display->setCursor((PANEL_RES_X - textWidth(text, size)) / 2, y);
  display->print(text);
}

// The line under the time: AM/PM in 12-hour time, and the seconds when wanted.
static void secondLine(const clockface::Settings &s, const struct tm &now, char *out) {
  out[0] = '\0';
  if (s.seconds) {
    snprintf(out, 8, ":%02d", now.tm_sec);
  }
  if (!s.hours24) {
    if (out[0]) {
      strcat(out, " ");
    }
    strcat(out, clockface::meridiem(now.tm_hour));
  }
}

// Digital: the built-in font, the time at twice the size.
static void drawDigital(const clockface::Settings &s, const struct tm &now, uint16_t main, uint16_t dim) {
  char date[12], time[6], below[12];
  clockface::formatDate(s.date, now.tm_wday, now.tm_mon + 1, now.tm_mday, date);
  clockface::formatTime(now.tm_hour, now.tm_min, s.hours24, time);
  secondLine(s, now, below);
  // Stack the lines and centre the stack: date 8, gap 8, time 14, gap 8, below 8.
  int height = 14 + (date[0] ? 16 : 0) + (below[0] ? 16 : 0);
  int y = (PANEL_RES_Y - height) / 2;
  if (date[0]) {
    printCentred(date, y, 1, dim);
    y += 16;
  }
  printCentred(time, y, 2, main);
  y += 22;
  if (below[0]) {
    printCentred(below, y, 1, dim);
  }
}

// Big: digits from a 3 x 5 pixel font, scaled to 12 x 30, across the panel.
static const uint8_t BIG_DIGITS[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};
static const int BIG_SX = 4, BIG_SY = 6;           // the scale: 12 x 30 pixels a digit
static const int BIG_W = 3 * BIG_SX, BIG_H = 5 * BIG_SY;
static const int BIG_GAP = 2, BIG_COLON = 2;

static void bigDigit(int digit, int x, int y, uint16_t color) {
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 3; col++) {
      if (BIG_DIGITS[digit][row] & (4 >> col)) {
        display->fillRect(x + col * BIG_SX, y + row * BIG_SY, BIG_SX, BIG_SY, color);
      }
    }
  }
}

static void drawBig(const clockface::Settings &s, const struct tm &now, uint16_t main, uint16_t dim) {
  char date[12], time[6], below[12];
  clockface::formatDate(s.date, now.tm_wday, now.tm_mon + 1, now.tm_mday, date);
  clockface::formatTime(now.tm_hour, now.tm_min, s.hours24, time);
  secondLine(s, now, below);
  int width = 0;
  for (const char *c = time; *c; c++) {
    width += (*c == ':' ? BIG_COLON : BIG_W) + (c[1] ? BIG_GAP : 0);
  }
  int height = BIG_H + (date[0] ? 12 : 0) + (below[0] ? 12 : 0);
  int y = (PANEL_RES_Y - height) / 2;
  if (date[0]) {
    printCentred(date, y, 1, dim);
    y += 12;
  }
  int x = (PANEL_RES_X - width) / 2;
  for (const char *c = time; *c; c++) {
    if (*c == ':') {
      display->fillRect(x, y + BIG_H / 3 - 1, BIG_COLON, BIG_COLON, main);
      display->fillRect(x, y + (BIG_H * 2) / 3 - 1, BIG_COLON, BIG_COLON, main);
      x += BIG_COLON + BIG_GAP;
    } else {
      bigDigit(*c - '0', x, y, main);
      x += BIG_W + BIG_GAP;
    }
  }
  if (below[0]) {
    printCentred(below, y + BIG_H + 4, 1, dim);
  }
}

// Analog: a dial with twelve marks and hands. The date, when on, is the day of
// the month in a window at three o'clock, as on a watch.
static const int CX = PANEL_RES_X / 2 - 1, CY = PANEL_RES_Y / 2 - 1, RADIUS = 30;

static void hand(float turns, int length, int width, uint16_t color) {
  float angle = turns * 2 * (float)M_PI;
  int x = CX + (int)lroundf(sinf(angle) * length);
  int y = CY - (int)lroundf(cosf(angle) * length);
  display->drawLine(CX, CY, x, y, color);
  if (width > 1) {
    // A second line beside it, offset across the hand's direction.
    int ox = fabsf(cosf(angle)) > 0.5f ? 0 : 1, oy = ox ? 0 : 1;
    display->drawLine(CX + ox, CY + oy, x + ox, y + oy, color);
  }
}

static void drawAnalog(const clockface::Settings &s, const struct tm &now, uint16_t main, uint16_t dim) {
  for (int i = 0; i < 12; i++) {
    float angle = i * (float)M_PI / 6;
    int x = CX + (int)lroundf(sinf(angle) * RADIUS), y = CY - (int)lroundf(cosf(angle) * RADIUS);
    if (i % 3 == 0) {
      display->fillRect(x - 1, y - 1, 2, 2, main); // twelve, three, six and nine
    } else {
      display->drawPixel(x, y, dim);
    }
  }
  if (s.date != clockface::DateFormat::Off) {
    char day[4];
    snprintf(day, sizeof(day), "%d", now.tm_mday);
    int w = textWidth(day, 1);
    int x = CX + 22 - w;
    display->drawRect(x - 2, CY - 5, w + 4, 11, dim);
    display->setTextSize(1);
    display->setTextColor(dim);
    display->setCursor(x, CY - 3);
    display->print(day);
  }
  float minutes = now.tm_min + now.tm_sec / 60.0f;
  hand(((now.tm_hour % 12) + minutes / 60) / 12, 15, 2, main);
  hand(minutes / 60, 25, 2, main);
  if (s.seconds) {
    hand(now.tm_sec / 60.0f, 27, 1, dim);
  }
  display->fillRect(CX - 1, CY - 1, 3, 3, main);
}

void ClockSlot::draw(const struct tm &now) {
  clockface::Settings s = clockSettings();
  shownSettings = clockface::pack(s);
  shownColor = clockface::packColor(s.custom);
  shownMinute = now.tm_min;
  shownSecond = now.tm_sec;

  clockface::Rgb color = clockface::colorAt(s.color, s.custom, now.tm_hour, now.tm_min);
  uint16_t main = rgb(color), dim = rgb(clockface::dimmed(color));
  display->clearScreen();
  display->setTextWrap(false);
  switch (s.style) {
    case clockface::Style::Big:
      drawBig(s, now, main, dim);
      break;
    case clockface::Style::Analog:
      drawAnalog(s, now, main, dim);
      break;
    default:
      drawDigital(s, now, main, dim);
      break;
  }
}
