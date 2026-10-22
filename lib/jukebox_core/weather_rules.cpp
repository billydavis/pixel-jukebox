#include "weather_rules.h"

#include <math.h>

namespace weather {

Icon iconFor(int code) {
  switch (code) {
    case 1000:
      return Icon::Clear;
    case 1003:
      return Icon::PartlyCloudy;
    case 1006:
    case 1009:
      return Icon::Cloudy;
    case 1030: // mist
    case 1135: // fog
    case 1147: // freezing fog
      return Icon::Fog;
    case 1150:
    case 1153:
      return Icon::Drizzle;
    case 1063: // patchy rain possible
    case 1180:
    case 1183:
    case 1186:
    case 1189:
    case 1192:
    case 1195:
    case 1240:
    case 1243:
    case 1246:
      return Icon::Rain;
    case 1066: // patchy snow possible
    case 1114: // blowing snow
    case 1117: // blizzard
    case 1210:
    case 1213:
    case 1216:
    case 1219:
    case 1222:
    case 1225:
    case 1255:
    case 1258:
      return Icon::Snow;
    case 1069: // sleet, freezing drizzle and rain, ice pellets
    case 1072:
    case 1168:
    case 1171:
    case 1198:
    case 1201:
    case 1204:
    case 1207:
    case 1237:
    case 1249:
    case 1252:
    case 1261:
    case 1264:
      return Icon::Sleet;
    case 1087:
    case 1273:
    case 1276:
    case 1279:
    case 1282:
      return Icon::Storm;
    default:
      // 1012-1048: haze, dust, sand, smoke and smog.
      if (code >= 1012 && code <= 1048) {
        return Icon::Haze;
      }
      return Icon::Cloudy;
  }
}

const char *iconName(Icon icon) {
  static const char *const NAMES[] = {"clear", "partly-cloudy", "cloudy", "haze", "fog",
                                      "drizzle", "rain", "snow", "sleet", "storm"};
  int i = (int)icon;
  return i >= 0 && i < (int)Icon::COUNT ? NAMES[i] : "cloudy";
}

bool hasNightPicture(Icon icon) { return icon == Icon::Clear || icon == Icon::PartlyCloudy; }

bool refreshValid(int minutes) { return minutes >= REFRESH_MIN && minutes <= REFRESH_MAX; }

bool fetchDue(bool changed, uint32_t now, uint32_t fetchedAt, uint32_t retryAt, int refreshMinutes) {
  if (changed) {
    return true;
  }
  if (retryAt != 0 && (int32_t)(now - retryAt) < 0) {
    return false;
  }
  if (fetchedAt == 0) {
    return true;
  }
  return now - fetchedAt >= (uint32_t)refreshMinutes * 60000UL;
}

Freshness freshness(uint32_t ageMs, int refreshMinutes) {
  if (ageMs > KEEP_MS) {
    return Freshness::Expired;
  }
  if (ageMs >= ((uint32_t)refreshMinutes + 5) * 60000UL) {
    return Freshness::Aged;
  }
  return Freshness::Fresh;
}

int roundTemperature(float degrees) {
  int rounded = (int)lroundf(degrees);
  return rounded == 0 ? 0 : rounded;
}

} // namespace weather
