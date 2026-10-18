#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

class MatrixPanel_I2S_DMA;

// Everything is drawn through this. It passes drawing on to the LED panel and
// keeps a copy of what is showing, so the picture can be read back. It can
// also be taken off the panel for a moment: drawing then goes to a hidden
// frame, which is how the next slot's first screen is prepared for a
// transition while the old one is still showing.
class Screen : public Adafruit_GFX {
 public:
  explicit Screen(MatrixPanel_I2S_DMA *panel);
  bool begin();

  void drawPixel(int16_t x, int16_t y, uint16_t colour) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t colour) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t colour) override;
  void fillScreen(uint16_t colour) override;
  void clearScreen();
  void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
  void setBrightness8(uint8_t brightness);
  int refreshHz() const; // how often the panel redraws, as the driver worked it out

  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  // false: drawing goes to a hidden frame, starting from black, and the panel
  // keeps showing what it had. true: drawing reaches the panel again; the
  // hidden frame is dropped, so show it with present() first if it is wanted.
  void setLive(bool live);

  // The frame being drawn to: what is on the panel, or the hidden frame.
  const uint16_t *pixels() const;

  // Puts a whole frame on the panel, repainting only the pixels that differ.
  void present(const uint16_t *frame);

 private:
  MatrixPanel_I2S_DMA *panel;
  uint16_t *shown = nullptr;  // what the panel is showing
  uint16_t *hidden = nullptr; // drawn to while not live
  bool live = true;
};

extern Screen *display;

// The colours used for status text: in progress, wrong, and fine.
namespace colour {
uint16_t amber();
uint16_t red();
uint16_t green();
} // namespace colour

bool setupDisplay();

// Brightness is 0-255 and is remembered across restarts. Call
// displaySetBrightness() from the display loop only. This is the day level:
// the night schedule shows other levels with displayApplyBrightness(), which
// is not saved.
uint8_t displayBrightness();
void displaySetBrightness(uint8_t value);
void displayApplyBrightness(uint8_t value);
uint8_t displayAppliedBrightness();

// Normal shows colours most accurately. Photo redraws the panel about five
// times as often, so cameras and phones film it without dark bands rolling
// through, at the cost of smooth dark shades. The driver's memory is laid out
// for one mode when it starts, so the mode is saved and used from the next
// start-up: change it with displaySaveMode(), then restart.
enum class PanelMode : uint8_t { Normal, Photo };
PanelMode displayMode();
const char *displayModeName(PanelMode mode);
void displaySaveMode(PanelMode mode);

void drawTestPattern();
void runColourCheck();
void showMessage(const char *title, const String &body, uint16_t colour);
void showRegistrationCode(const String &code);
