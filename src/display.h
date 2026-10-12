#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

extern MatrixPanel_I2S_DMA *display;

bool setupDisplay();
void drawTestPattern();
void showMessage(const char *title, const String &body, uint16_t colour);
void showRegistrationCode(const String &code);
