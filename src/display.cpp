#include "display.h"

#include "pins.h"

static const uint8_t BRIGHTNESS = 32; // 0-255; white tints yellow at 64 and red at 128 on the current supply

MatrixPanel_I2S_DMA *display = nullptr;

bool setupDisplay() {
  HUB75_I2S_CFG::i2s_pins pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN,
                                  B_PIN,  C_PIN,  D_PIN,  E_PIN,  LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG config(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
  config.clkphase = false; // this panel shifts one pixel sideways with the default

  display = new MatrixPanel_I2S_DMA(config);
  if (!display->begin()) {
    return false;
  }
  display->setBrightness8(BRIGHTNESS);
  display->clearScreen();
  return true;
}

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

  display->setBrightness8(BRIGHTNESS);
  display->clearScreen();
}
