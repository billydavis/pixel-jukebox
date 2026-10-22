#pragma once

#define DEVICE_MODEL "pixel-jukebox-64x64"
#define FIRMWARE_VERSION "0.39.0"
#define USER_AGENT "pixel-jukebox/" FIRMWARE_VERSION

#define MAKAPIX_API_URL "https://makapix.club/api"
// A post's page is this plus its storage key; the site redirects to its /p/ address.
#define MAKAPIX_POST_PAGE_URL "https://makapix.club/post/"

// The board answers at http://<MDNS_HOSTNAME>.local
#define MDNS_HOSTNAME "pixel-jukebox"

// The time zone is chosen in Settings > Clock (lib/jukebox_core/timezones.h);
// US Central until it is.
#define NTP_SERVER "pool.ntp.org"
