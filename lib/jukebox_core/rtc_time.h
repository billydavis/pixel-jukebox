#pragma once

#include <stdint.h>

// The time registers of the PCF85063 clock chip, kept apart from I2C so the
// conversion can be tested. The chip holds UTC, 24-hour, years 2000-2099, as
// seven BCD registers from 0x04:
//
//   seconds (bit 7: the oscillator stopped, so the time is not to be trusted),
//   minutes, hours, day of month, weekday (0 = Sunday), month, year (00-99)

namespace rtc {

static const int REGISTERS = 7;

// The time in `regs` as Unix seconds. False when the oscillator-stopped flag
// is set or any field is out of range, as after a power loss with no battery.
bool decode(const uint8_t regs[REGISTERS], uint32_t &unixSeconds);

// Unix seconds as register values, with the oscillator-stopped flag clear.
// Times before 2000 or after 2099 are clamped to the range.
void encode(uint32_t unixSeconds, uint8_t regs[REGISTERS]);

// Days since 1970-01-01 for a date, and back; used by both of the above.
int32_t daysFromCivil(int year, int month, int day);
void civilFromDays(int32_t days, int &year, int &month, int &day);

} // namespace rtc
