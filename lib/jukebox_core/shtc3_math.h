#pragma once

#include <stddef.h>
#include <stdint.h>

// The arithmetic of the board's Sensirion SHTC3 temperature and humidity
// sensor, kept apart from I2C so it can be tested. A measurement arrives as
// six bytes: temperature (two bytes and a checksum), then humidity (the same).

namespace shtc3 {

// Sensirion's CRC-8: polynomial 0x31, starting at 0xFF.
uint8_t crc(const uint8_t *data, size_t length);

// Degrees C and percent from a measurement. False if a checksum is wrong.
bool decode(const uint8_t bytes[6], float &celsius, float &humidity);

// True if `id` (from the read-ID command, checksum already checked) is an SHTC3.
bool isShtc3(uint16_t id);

} // namespace shtc3
