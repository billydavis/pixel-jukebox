#pragma once

#include <Arduino.h>

// Speaker output through the board's ES8311 codec and microphone input through
// its ES7210 ADC (I2C control, I2S audio at 16 kHz).

// Starts the I2C bus, both codecs and I2S in both directions. Safe to call
// more than once; later calls report whether the first one worked.
bool audioBegin();

// 0-100.
void audioSetVolume(int percent);

// Plays a sine tone and returns when it has finished.
void audioTone(float frequencyHz, uint32_t durationMs);

// Reads the next `count` samples from each microphone, waiting for them to arrive.
bool audioRead(int16_t *left, int16_t *right, int count);

// Discards input that built up while nobody was reading.
void audioFlushInput();

// Loudness of the two microphones over a stretch of time, on a 0-32767 scale.
struct MicLevels {
  int rmsLeft, rmsRight;
  int peakLeft, peakRight;
  int frames; // samples measured; 0 means no audio arrived
};
MicLevels audioMeasure(uint32_t durationMs);

// Lists the devices that answer on the board's I2C bus (codec, sensors, RTC).
String audioScanI2c();
