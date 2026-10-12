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
