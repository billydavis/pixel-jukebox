# Backlog

Everything planned or suggested so far, in one place. Size is a rough guess:
**S** is an evening, **M** a few sessions, **L** a project of its own.
Priority is a suggestion until it has been agreed.

## Makapix

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 1 | ~~Like button on the control page~~ | S | Done (0.14.0) | The board passes the like to Makapix (the token never reaches the page) and remembers what has been liked, so the heart shows filled when an artwork comes round again |
| 2 | ~~View reports~~ | S | Done (0.14.0) | One call per artwork shown; feeds artists' view counts and trending. Views from offline play saved and sent later |
| 3 | ~~Channels~~ | S-M | Done (0.18.0) | One per artwork slot in the rotation: everything, promoted, mine, an artist, a hashtag, liked by a name |
| 4 | ~~Playsets~~ | M | Closed | Not worth it: Makapix has one playset, `followed_artists`, and the website has no button to send it (only its API can). An artwork slot per artist in the rotation does nearly the same, with turns and pausing on top. Note: the panel keeps 8 channels warm (`MAX_FEEDS`); more than 8 artwork channels still work but restart their feed each turn |
| 5 | ~~Live presence and remote commands (MQTT)~~ | L | Done (0.29.0) | Online on makapix.club with the artwork showing; Next, Back, send to player and play channel from the website. Certificate fetched and renewed by the panel. Playsets are #4 |
| 35 | ~~One "what's on screen" layer for overrides~~ | M | Done (0.29.0) | `lib/jukebox_core/screen_rules.h`; show works in sound mode now, and Back and show-any-post came with it. Original note: | Sound mode, night clock-only and off, and "show this artwork" each take over the screen with their own special cases in `main.cpp` (Hold and Show refused in sound mode, Next meaning something else). MQTT's "show this post" would be a fourth. Make them all overrides of the rotation in one place first. From a review of `/mode`, 2026-10-18 |
| 6 | ~~Comments on the control page~~ | S | Closed, replaced (0.35.0) | Low value on a 64x64 panel, and the player API has no comments. Replaced by a link to the artwork's page on makapix.club, where its comments are: an icon beside the heart on the Now tab, and Makapix in the Art tab's saved-art bar. The player API gives no short `/p/` code, so the link is `makapix.club/post/<storage key>`, which the site redirects; the card's index now keeps the key (art saved before 0.35.0 has no link until it is downloaded again) |

## Panel and hardware

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 37 | ~~Colors look bleached out in photo mode~~ | M | Done (0.32.0) | The display library gave its low color bits equal time instead of 1, 2, 4..., worst in photo mode, plus a faint ghost of the row above; patched in `lib/ESP32-HUB75-MatrixPanel-DMA` (`docs/color.md`). Confirmed on the panel 2026-10-19: "100% better". White checked neutral, so no white-balance adjustment |
| 38 | ~~Test pattern in Settings~~ | S | Done (0.32.1) | Settings > Display > Test pattern: ramps, dark steps, color bars, white (`/api/v1/test`). Checked on the panel 2026-10-19: dark steps climb in order in both modes (bright ones are harder to tell apart, which is the eye, not a fault) and white looks neutral. So p64's low-brightness refinements and a white-balance adjustment are not needed |
| 7 | Tap to like | S | On hold | On hold by choice (2026-10-21): taps may not be the best way to like. Measured in the case with the `motion-test` build: the case absorbs most of a tap, the firmest reaching 0.84 g and most 0.2-0.5 g, against the detector's default of 1.5 g (from p64, a bare panel), so nothing registered. Bumps were not told apart from taps in that run; if it is picked up again, run a labelled test (taps, double taps, then bumps, 30 s each, logging down to 0.05 g) before choosing a threshold. Original plan: double tap = like, a heart on the panel, a tap to undo |
| 8 | ~~Panel orientation setting~~ | S | Done (0.34.0) | Settings > Display: rotate 0, 90, 180 or 270 degrees clockwise, for however the panel sits in the case, and Mirror (left and right swapped), for a live stream or a mirror. Changes at once, saved, applied before the start-up pattern. Done in `Screen` (`lib/jukebox_core/orientation.h` does the mapping), so the picture read back for transitions stays upright; `rotation` and `mirror` in `GET`/`PUT /panel`. Was auto-rotation from the motion sensor, not needed in a fixed case (owner, 2026-10-19). Checked on the panel 2026-10-19 |
| 9 | ~~Keep time with the onboard clock chip~~ | S | Done (0.15.0) | Keeps time through restarts. Through power cuts only with a backup battery on J4 (CR2032 ordered; test by unplugging the panel, then check `keptTimeAtStart`) |
| 10 | ~~Day and night brightness schedule~~ | S-M | Done (0.19.0) | Dim, clock only or off between two times, with fades and an hour's override. Later: follow sunset and sunrise from a rough location |
| 11 | ~~Room temperature and humidity~~ | S | Done differently (0.20.0) | The sensor sits in the warm case (p64 measured 31.8 C and 33 % there), so it cannot give room readings and no external sensor will be added. It reports the case temperature and humidity in `/diagnostics` instead |
| 12 | ~~Weather slot~~ | M | Done (0.36.0) | Current conditions from WeatherAPI.com (chosen over Open-Meteo, which p64 uses, and Visual Crossing; free plan, 100,000 calls a month), over HTTPS: a pixel-art picture (10 kinds, night versions for clear and partly cloudy, `tools/make_weather_icons.py`) and the temperature. Checked every 15 to 60 minutes (30 by default; 60 is the most their terms allow a reading to be kept), only while a weather slot is in the rotation and the panel is not off for the night; the age shows once a check is missed, "no data" after 60 minutes. Settings > Weather: town search, F or C, refresh, the key (never sent back; or `WEATHERAPI_KEY` in `secrets.h`). Credit "Weather by WeatherAPI.com", required on the free plan |
| 13 | Use the speaker | S | Deferred | For example a chime on a like or on the hour. Deferred by choice (2026-10-21) |
| 14 | ~~Raise the default brightness~~ | S | Closed | Not wanted: the panel looks better dim (the owner runs it at about 51 of 255), and lower brightness means less heat and slower LED ageing. Below about 20 the darkest shades start to vanish |
| 15 | ~~Shorten the 40 s wait before playing offline at boot~~ | S | Done (0.30.3) | With art on the card, one 8 s attempt, then it plays from the card and keeps trying every minute in the background. Without a card, still two 20 s attempts before the setup hotspot |
| 28 | ~~Starter art~~ | M | Done (0.33.0) | 28 pieces built into the firmware (1.1 MB): from five CC0 pixel art packs (licences checked three ways, `docs/starter-art.md`) and drawn by code, credited in `CREDITS.md`. A "starter" channel, and the art slot plays them whenever it has nothing else: before linking (turns go between the code and starter art), offline with an empty card, a channel with a problem. Embedded rather than in the unused 23 MB storage area, so a new panel has it with no second flashing step; that area is still free |
| 39 | ~~Slow Wi-Fi with photo mode on~~ | S | Done (0.33.0) | Every response went out at 15-50 KB/s and pings took up to 712 ms: the 14 panel pins, driven at full strength (~40 mA) at photo mode's 16 MHz clock, jammed the board's own 2.4 GHz radio. `setupDisplay()` now sets them to ~10 mA: 190-360 KB/s, pings 4-31 ms, the 28 starter GIFs load in 5 s instead of 50. Picture checked on the panel 2026-10-19: unchanged |
| 29 | Auto-dim when the room is quiet | S | Deferred | The board has no light sensor, so it cannot tell dark; it can tell quiet, from the microphones. The schedule (#10) covers night. A light sensor on the I2C connector would be a hardware project. Deferred by choice (2026-10-18): not wanted for now |
| 32 | ~~Photo mode~~ | S | Done (0.17.0) | About 520 Hz instead of 100 so cameras film without bands; restarts to switch. Open: check the picture at the faster clock, and whether normal mode should run at 16 MHz too (200 Hz, same colors) |

## Control page, API and apps

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 16 | ~~Split the control page into pages~~ | M | Done (0.25.0) | Done as a redesign: one page with a now-playing header and tabs (Now, Rotation, Art, Night, Settings), the gallery folded in as the Art tab, two columns on wide screens, and the Wi-Fi setup pages restyled to match. Original note: decide the layout as features arrive; includes where Restart lives. Lessons so far: the Night schedule's On/Off switch was easy to miss next to "Turn the panel off" (now any night change switches it on, but the layout should make on/off obvious); settings that need a restart (photo mode) or a confirm (clear all, restart) want a consistent pattern |
| 17 | ~~Upload your own art to the card~~ | M | Done (0.31.0) | GIFs only, 64 x 64 or less, up to 200, in `jukebox/uploads/`; the "uploads" channel plays them. Later, if wanted: PNG and JPEG converted to GIF in the browser |
| 18 | ~~A PIN for the control page and API~~ | M | Done (0.26.0) | Optional, 4 to 8 digits, salted SHA-256 in flash. Sessions by cookie (kept through restarts), `X-Jukebox-Pin` for scripts, 30 s lockout after five wrong. Recovery: the button's Wi-Fi setup page offers to remove it. Modelled on p64's design |
| 19 | ~~Updates over Wi-Fi (OTA)~~ | M | Done (0.28.0) | `POST /update` from the PC (`tools/ota_upload.py`, or Settings), written to the spare firmware slot, kept only after 30 s with the API up, else rolled back |
| 36 | Install updates from GitHub releases | M | Later | The panel checks the latest release and installs it, as p64 does. Needs releases made (one `gh release create` per version) and GitHub's root certificates on the board. Only useful away from the PC or with a second panel |
| 20 | ~~A "Liked" channel~~ | S | Done (0.18.0) | `likedby:<your name>` plays everything you have reacted to, from Makapix itself (website likes included) |
| 21 | A unique name per panel, for a second unit | S | Deferred | Every board is `pixel-jukebox.local` today. Deferred by choice (2026-10-21) |
| 22 | ~~Companion app (Flutter)~~ | L | Closed | Not wanted (2026-10-21): the panel is only used on the home network, where the control page does everything. An installable web app (PWA) was considered instead and dropped: browsers only install one over HTTPS, and the panel serves plain http on the LAN (iPhone's Add to Home Screen would have worked; Android and desktop would not). Kept from it: the logo as the favicon (0.35.1) |
| 30 | Explore Bluetooth (BLE) | M | Later | A learning project. Ideas: Wi-Fi setup from a phone instead of the hotspot, or the companion app finding and controlling the panel without Wi-Fi |

## Slots and effects

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 23 | More visualizers | S each | 5 | Done in 0.30.0: pulse rings, starfield, stereo scope, hi-fi meter, and a fire that follows the beat (`beat_follower.h`). Left: artwork that dances to the beat (passed over for now). A waterfall was tried and dropped: too faint to read on the panel |
| 24 | ~~Built-in effects: falling code, rainbow, Game of Life~~ | S each | Closed | Covered by the starter art (#28): Game of Life, a starfield, a color wheel, plasma and more play as built-in GIFs. Effects drawn live on the panel were not wanted on top (2026-10-19) |
| 25 | ~~Per-slot settings, with more clock options~~ | M | Done (0.37.0) | Split in two (2026-10-22). Per row in the Rotation tab: how long a timed slot (the clock, the weather) stays on screen, 5 to 60 seconds (`seconds` in `/slots`, 10 by default). One look for the whole panel in Settings > Clock (`/clock`), also used on clock-only nights: Digital, Big (chunky 12 x 30 digits) or Analog (a dial with hands, the day in a window); 12 or 24 hours; seconds; the date as "Thu Oct 22", "22 Oct", "10/22" or none; white, amber, mint, time of day (dim red at night, orange at dawn, white at midday, amber in the evening) or a custom color from a picker. Show on panel previews it |

## Housekeeping

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 26 | ~~Refresh the README roadmap~~ | S | Done (0.26.0) | The roadmap now points here instead of keeping its own list, and the Development section describes folders rather than every file |
| 27 | CI build on GitHub | S | Deferred | Deferred by choice in the architecture plan |
| 31 | ~~Crash records in diagnostics~~ | S | Done (0.16.0) | `GET /crash`, `tools/decode_crash.py`. One record from an earlier, unarchived build was found: the loader, a null-pointer write while freeing memory. Watch for it again |
| 33 | ~~Memory headroom review~~ | S-M | Done (0.21.0) | TLS memory moved to PSRAM (lowest free 49 -> 97 KB in normal use); audio released when sound mode ends (18 KB back); one permanent loader task instead of a 16 KB stack per artwork; floors of 48 KB free and a 32 KB block in the smoke test. It also found and fixed the crash behind both crash records: a card file read in one task while a TLS request ran in another (see `storage/card.h`) |
| 34 | ~~Start dark at night~~ | S | Done (0.20.2) | At night the panel starts at the night level (dark when off) and skips the test pattern; Wi-Fi setup still shows at day brightness |
