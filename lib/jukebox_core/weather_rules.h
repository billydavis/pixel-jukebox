#pragma once

#include <stdint.h>

// The weather slot's decisions (backlog #12), kept apart from the network so
// they can be tested. The data comes from WeatherAPI.com's current conditions.

namespace weather {

// The pictures the panel has, one per kind of weather. WeatherAPI.com has 60
// condition codes; each belongs to one of these.
enum class Icon : uint8_t { Clear, PartlyCloudy, Cloudy, Haze, Fog, Drizzle, Rain, Snow, Sleet, Storm, COUNT };

// The picture for a WeatherAPI.com condition code
// (https://www.weatherapi.com/docs/weather_conditions.json). An unknown code
// is shown as cloudy rather than as nothing.
Icon iconFor(int code);
const char *iconName(Icon icon);
// Clear and partly cloudy look different at night (a moon instead of the sun).
bool hasNightPicture(Icon icon);

// Refresh, in minutes. WeatherAPI.com's terms let current conditions be kept
// for 60 minutes at most, so that is the longest; 15 keeps well inside the
// free plan (about 2,900 calls a month against 100,000).
static const int REFRESH_MIN = 15;
static const int REFRESH_MAX = 60;
static const int REFRESH_DEFAULT = 30;
bool refreshValid(int minutes);

// After a failed fetch: try again after this long. A rejected key or a used-up
// quota waits longer, since retrying soon will not help.
static const uint32_t RETRY_MS = 5UL * 60 * 1000;
static const uint32_t RETRY_REFUSED_MS = 60UL * 60 * 1000;

// Whether to fetch now, on a millisecond clock. `changed`: the place, the key
// or the units changed since the last reading, so fetch at once. `fetchedAt`
// is when the last reading arrived (0 for none); `retryAt` is the earliest
// moment after a failure (0 for none).
bool fetchDue(bool changed, uint32_t now, uint32_t fetchedAt, uint32_t retryAt, int refreshMinutes);

// How a reading `ageMs` old may be shown. Fresh: as it is. Aged: with its age,
// once it is a refresh and five minutes old (a fetch has been missed).
// Expired: past the 60 minutes the terms allow; show "no data" instead.
enum class Freshness : uint8_t { Fresh, Aged, Expired };
Freshness freshness(uint32_t ageMs, int refreshMinutes);
static const uint32_t KEEP_MS = 60UL * 60 * 1000;

// The temperature as shown: rounded to a whole degree, -0 shown as 0.
int roundTemperature(float degrees);

} // namespace weather
