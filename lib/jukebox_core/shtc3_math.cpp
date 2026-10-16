#include "shtc3_math.h"

namespace shtc3 {

uint8_t crc(const uint8_t *data, size_t length) {
  uint8_t value = 0xFF;
  for (size_t i = 0; i < length; i++) {
    value ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      value = (value & 0x80) ? (uint8_t)((value << 1) ^ 0x31) : (uint8_t)(value << 1);
    }
  }
  return value;
}

bool decode(const uint8_t bytes[6], float &celsius, float &humidity) {
  if (crc(bytes, 2) != bytes[2] || crc(bytes + 3, 2) != bytes[5]) {
    return false;
  }
  uint16_t rawT = (uint16_t)(bytes[0] << 8 | bytes[1]);
  uint16_t rawH = (uint16_t)(bytes[3] << 8 | bytes[4]);
  celsius = -45.0f + 175.0f * rawT / 65536.0f;
  humidity = 100.0f * rawH / 65536.0f;
  return true;
}

bool isShtc3(uint16_t id) { return (id & 0x083F) == 0x0807; }

} // namespace shtc3
