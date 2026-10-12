#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

extern MatrixPanel_I2S_DMA *display;

// The colours used for status text: in progress, wrong, and fine.
namespace colour {
uint16_t amber();
uint16_t red();
uint16_t green();
} // namespace colour

bool setupDisplay();

// Brightness is 0-255 and is remembered across restarts. Call
// displaySetBrightness() from the display loop only.
uint8_t displayBrightness();
void displaySetBrightness(uint8_t value);

void drawTestPattern();
void runColourCheck();
void showMessage(const char *title, const String &body, uint16_t colour);
void showRegistrationCode(const String &code);
