#pragma once

#define DEVICE_MODEL "pixel-jukebox-64x64"
#define FIRMWARE_VERSION "0.27.0"
#define USER_AGENT "pixel-jukebox/" FIRMWARE_VERSION

#define MAKAPIX_API_URL "https://makapix.club/api"

// The board answers at http://<MDNS_HOSTNAME>.local
#define MDNS_HOSTNAME "pixel-jukebox"

// POSIX time zone for the clock (US Central here). Other zones:
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE "CST6CDT,M3.2.0,M11.1.0"
#define NTP_SERVER "pool.ntp.org"
