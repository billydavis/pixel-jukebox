#pragma once

#include <Arduino.h>

// Speaker output through the board's ES8311 codec (I2C control, I2S audio).

// Starts the I2C bus, the codec, the I2S output and the speaker amplifier.
bool audioBegin();

// 0-100.
void audioSetVolume(int percent);

// Plays a sine tone and returns when it has finished.
void audioTone(float frequencyHz, uint32_t durationMs);

// Lists the devices that answer on the board's I2C bus (codec, sensors, RTC).
String audioScanI2c();
