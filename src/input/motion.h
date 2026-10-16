#pragma once

#include <stdint.h>

// The board's QMI8658 motion sensor, on the same I2C bus as the audio chips.
// Only the accelerometer is used: +-8 g at 500 Hz, read by polling.

// Looks for the sensor and starts it. False when it is not there.
bool motionBegin();
bool motionPresent();
// The I2C address it answered on, or 0.
uint8_t motionAddress();
// The latest sample in g. False on a bus error.
bool motionRead(float &ax, float &ay, float &az);
