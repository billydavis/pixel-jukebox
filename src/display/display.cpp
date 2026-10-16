#include "display/display.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Preferences.h>

#include "pins.h"

// 0-255. The starting value on a new board; a brightness set through the API
// is saved and used instead.
static const uint8_t DEFAULT_BRIGHTNESS = 32;

static const int W = PANEL_RES_X;
static const int H = PANEL_RES_Y;
static const size_t FRAME_BYTES = W * H * sizeof(uint16_t);

static Preferences prefs;
static uint8_t brightness = DEFAULT_BRIGHTNESS;

Screen *display = nullptr;

uint16_t colour::amber() { return Screen::color565(255, 200, 0); }
uint16_t colour::red() { return Screen::color565(255, 0, 0); }
uint16_t colour::green() { return Screen::color565(0, 255, 0); }

Screen::Screen(MatrixPanel_I2S_DMA *panel) : Adafruit_GFX(W, H), panel(panel) {}

bool Screen::begin() {
  if (!panel->begin()) {
    return false;
  }
  // The frame copies live in external memory, which has plenty to spare.
  shown = (uint16_t *)ps_calloc(1, FRAME_BYTES);
  hidden = (uint16_t *)ps_calloc(1, FRAME_BYTES);
  return shown && hidden;
}

void Screen::drawPixel(int16_t x, int16_t y, uint16_t colour) {
  if (x < 0 || y < 0 || x >= W || y >= H) {
    return;
  }
  if (live) {
    shown[y * W + x] = colour;
    panel->drawPixel(x, y, colour);
  } else {
    hidden[y * W + x] = colour;
  }
}

void Screen::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colour) {
  int x0 = max((int)x, 0);
  int y0 = max((int)y, 0);
  int x1 = min(x + w, W);
  int y1 = min(y + h, H);
  if (x0 >= x1 || y0 >= y1) {
    return;
  }
  uint16_t *frame = live ? shown : hidden;
  for (int row = y0; row < y1; row++) {
    for (int column = x0; column < x1; column++) {
      frame[row * W + column] = colour;
    }
  }
  if (live) {
    panel->fillRect(x0, y0, x1 - x0, y1 - y0, colour);
  }
}

void Screen::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t colour) {
  fillRect(x, y, w, 1, colour);
}

void Screen::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t colour) {
  fillRect(x, y, 1, h, colour);
}

void Screen::fillScreen(uint16_t colour) { fillRect(0, 0, W, H, colour); }

void Screen::clearScreen() {
  if (live) {
    memset(shown, 0, FRAME_BYTES);
    panel->clearScreen();
  } else {
    memset(hidden, 0, FRAME_BYTES);
  }
}

void Screen::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
  drawPixel(x, y, color565(r, g, b));
}

void Screen::setBrightness8(uint8_t value) { panel->setBrightness8(value); }

int Screen::refreshHz() const { return panel->calculated_refresh_rate; }

void Screen::setLive(bool nowLive) {
  if (!nowLive) {
    memset(hidden, 0, FRAME_BYTES);
  }
  live = nowLive;
}

const uint16_t *Screen::pixels() const { return live ? shown : hidden; }

void Screen::present(const uint16_t *frame) {
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      uint16_t colour = frame[y * W + x];
      if (shown[y * W + x] != colour) {
        shown[y * W + x] = colour;
        panel->drawPixel(x, y, colour);
      }
    }
  }
}

static PanelMode mode = PanelMode::Normal;

bool setupDisplay() {
  prefs.begin("jukebox", false);
  mode = prefs.getBool("photo", false) ? PanelMode::Photo : PanelMode::Normal;

  HUB75_I2S_CFG::i2s_pins pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN,
                                  B_PIN,  C_PIN,  D_PIN,  E_PIN,  LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG config(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
  config.clkphase = false; // this panel shifts one pixel sideways with the default
  if (mode == PanelMode::Photo) {
    // Twice the clock, and the driver gives the five dimmest bits of each
    // colour the same short time instead of three, for about 520 Hz instead
    // of about 100. The driver blanks longer around each latch at this speed.
    config.i2sspeed = HUB75_I2S_CFG::HZ_16M;
    config.latch_blanking = 8;
    config.min_refresh_rate = 480;
  }

  display = new Screen(new MatrixPanel_I2S_DMA(config));
  if (!display->begin()) {
    return false;
  }
  Serial.printf("Panel: %s mode, %d Hz\n", displayModeName(mode), display->refreshHz());
  brightness = prefs.getUChar("brightness", DEFAULT_BRIGHTNESS);
  displayApplyBrightness(brightness);
  display->clearScreen();
  return true;
}

uint8_t displayBrightness() { return brightness; }

PanelMode displayMode() { return mode; }

const char *displayModeName(PanelMode value) { return value == PanelMode::Photo ? "photo" : "normal"; }

bool displayModeFromName(const String &name, PanelMode &value) {
  if (name == "normal" || name == "photo") {
    value = name == "photo" ? PanelMode::Photo : PanelMode::Normal;
    return true;
  }
  return false;
}

void displaySaveMode(PanelMode value) { prefs.putBool("photo", value == PanelMode::Photo); }

static uint8_t applied = 0; // what the panel is showing now

void displaySetBrightness(uint8_t value) {
  if (value == brightness) {
    return;
  }
  brightness = value;
  displayApplyBrightness(brightness);
  prefs.putUChar("brightness", brightness);
}

void displayApplyBrightness(uint8_t value) {
  applied = value;
  display->setBrightness8(value);
}

uint8_t displayAppliedBrightness() { return applied; }

// Colour gradient with a marker in each corner, to check wiring and orientation:
// red = top-left, green = top-right, blue = bottom-left, white = bottom-right.
void drawTestPattern() {
  for (int y = 0; y < PANEL_RES_Y; y++) {
    for (int x = 0; x < PANEL_RES_X; x++) {
      display->drawPixelRGB888(x, y, x * 4, y * 4, 255 - x * 4);
    }
  }
  display->fillRect(0, 0, 4, 4, display->color565(255, 0, 0));
  display->fillRect(PANEL_RES_X - 4, 0, 4, 4, display->color565(0, 255, 0));
  display->fillRect(0, PANEL_RES_Y - 4, 4, 4, display->color565(0, 0, 255));
  display->fillRect(PANEL_RES_X - 4, PANEL_RES_Y - 4, 4, 4, display->color565(255, 255, 255));
}

void showMessage(const char *title, const String &body, uint16_t colour) {
  display->clearScreen();
  display->setTextSize(1);
  display->setTextWrap(true);
  display->setCursor(0, 0);
  display->setTextColor(colour);
  display->println(title);
  display->setTextColor(display->color565(255, 255, 255));
  display->println(body);
}

// The 6-character code is split over two large lines so it fits 64 pixels.
void showRegistrationCode(const String &code) {
  display->clearScreen();
  display->setTextWrap(false);

  display->setTextSize(1);
  display->setTextColor(display->color565(255, 200, 0));
  display->setCursor(2, 2);
  display->print("Enter code");
  display->setCursor(2, 54);
  display->print("in Makapix");

  display->setTextSize(2);
  display->setTextColor(display->color565(255, 255, 255));
  display->setCursor(14, 16);
  display->print(code.substring(0, 3));
  display->setCursor(14, 34);
  display->print(code.substring(3));
}

// White at rising brightness, then colour bars. If white turns pink as the
// brightness rises, the 5V supply is sagging (red LEDs need the least voltage).
void runColourCheck() {
  const uint8_t levels[] = {32, 64, 128};
  for (uint8_t level : levels) {
    display->setBrightness8(level);
    display->fillScreen(display->color565(255, 255, 255));
    display->setTextSize(1);
    display->setTextColor(0);
    display->setCursor(2, 2);
    display->print(level);
    delay(8000);
  }

  const uint8_t bars[][3] = {{255, 255, 255}, {128, 128, 128}, {64, 64, 64},
                             {255, 0, 0},     {0, 255, 0},     {0, 0, 255}};
  const int barHeight = PANEL_RES_Y / 6;
  display->clearScreen();
  for (int i = 0; i < 6; i++) {
    display->fillRect(0, i * barHeight, PANEL_RES_X, barHeight,
                      display->color565(bars[i][0], bars[i][1], bars[i][2]));
  }
  delay(12000);

  display->setBrightness8(brightness);
  display->clearScreen();
}
