#pragma once

// HUB75 wiring of the Waveshare ESP32-S3-RGB-Matrix driver board.
// Source: https://docs.waveshare.com/ESP32-Peripheral-Tutorials/Display/LED-Matrix

#define PANEL_RES_X 64
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

#define R1_PIN 4
#define G1_PIN 5
#define B1_PIN 6
#define R2_PIN 7
#define G2_PIN 15
#define B2_PIN 16
#define A_PIN 18
#define B_PIN 8
#define C_PIN 3
#define D_PIN 42
#define E_PIN 9 // required for 1/32 scan panels such as 64x64
#define LAT_PIN 40
#define OE_PIN 2
#define CLK_PIN 41

// Audio (ES8311 codec) and the shared I2C bus. Source: community pin map checked
// against Waveshare's BSP, https://github.com/NickoScope/waveshare-rgb-matrix-p2-64x64
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define I2S_MCLK_PIN 12
#define I2S_BCLK_PIN 43
#define I2S_WS_PIN 38
#define I2S_DOUT_PIN 21
#define SPEAKER_AMP_PIN 11 // high = amplifier on

#define BUTTON_PIN 0 // BOOT button, low when pressed
