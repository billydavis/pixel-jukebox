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
| 6 | Comments on the control page | S | 5 | Low value on a 64x64 panel |

## Panel and hardware

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 7 | **Tap to like** | S | Blocked | Waiting for the case. Run the `motion-test` build again once mounted, then pick the threshold. Plan: double tap = like, a heart on the panel, a tap to undo |
| 8 | Auto-rotation from the motion sensor | M | Blocked | Also needs the case, to know how the panel stands |
| 9 | ~~Keep time with the onboard clock chip~~ | S | Done (0.15.0) | Keeps time through restarts. Through power cuts only with a backup battery on J4 (CR2032 ordered; test by unplugging the panel, then check `keptTimeAtStart`) |
| 10 | ~~Day and night brightness schedule~~ | S-M | Done (0.19.0) | Dim, clock only or off between two times, with fades and an hour's override. Later: follow sunset and sunrise from a rough location |
| 11 | ~~Room temperature and humidity~~ | S | Done differently (0.20.0) | The sensor sits in the warm case (p64 measured 31.8 C and 33 % there), so it cannot give room readings and no external sensor will be added. It reports the case temperature and humidity in `/diagnostics` instead |
| 12 | Weather slot | M | 4 | Needs a weather service. The only way to show a temperature that means anything, since room readings are out |
| 13 | Use the speaker | S | 5 | For example a chime on a like or on the hour |
| 14 | ~~Raise the default brightness~~ | S | Closed | Not wanted: the panel looks better dim (the owner runs it at about 51 of 255), and lower brightness means less heat and slower LED ageing. Below about 20 the darkest shades start to vanish |
| 15 | ~~Shorten the 40 s wait before playing offline at boot~~ | S | Done (0.30.3) | With art on the card, one 8 s attempt, then it plays from the card and keeps trying every minute in the background. Without a card, still two 20 s attempts before the setup hotspot |
| 28 | Starter art in the spare flash | M | 4 | The firmware reserves a 23 MB storage area it never uses. A few dozen built-in pieces would give a new panel something to play before it is on Wi-Fi or registered, and offline without a card |
| 29 | Auto-dim when the room is quiet | S | 4 | The board has no light sensor, so it cannot tell dark; it can tell quiet, from the microphones. The schedule (#10) covers night. A light sensor on the I2C connector would be a hardware project |
| 32 | ~~Photo mode~~ | S | Done (0.17.0) | About 520 Hz instead of 100 so cameras film without bands; restarts to switch. Open: check the picture at the faster clock, and whether normal mode should run at 16 MHz too (200 Hz, same colours) |

## Control page, API and apps

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 16 | ~~Split the control page into pages~~ | M | Done (0.25.0) | Done as a redesign: one page with a now-playing header and tabs (Now, Rotation, Art, Night, Settings), the gallery folded in as the Art tab, two columns on wide screens, and the Wi-Fi setup pages restyled to match. Original note: decide the layout as features arrive; includes where Restart lives. Lessons so far: the Night schedule's On/Off switch was easy to miss next to "Turn the panel off" (now any night change switches it on, but the layout should make on/off obvious); settings that need a restart (photo mode) or a confirm (clear all, restart) want a consistent pattern |
| 17 | Upload your own art to the card ("My art") | M | 4 | Unblocked now #18 is done: without a PIN anyone on the Wi-Fi could fill the card |
| 18 | ~~A PIN for the control page and API~~ | M | Done (0.26.0) | Optional, 4 to 8 digits, salted SHA-256 in flash. Sessions by cookie (kept through restarts), `X-Jukebox-Pin` for scripts, 30 s lockout after five wrong. Recovery: the button's Wi-Fi setup page offers to remove it. Modelled on p64's design |
| 19 | ~~Updates over Wi-Fi (OTA)~~ | M | Done (0.28.0) | `POST /update` from the PC (`tools/ota_upload.py`, or Settings), written to the spare firmware slot, kept only after 30 s with the API up, else rolled back |
| 36 | Install updates from GitHub releases | M | Later | The panel checks the latest release and installs it, as p64 does. Needs releases made (one `gh release create` per version) and GitHub's root certificates on the board. Only useful away from the PC or with a second panel |
| 20 | ~~A "Liked" channel~~ | S | Done (0.18.0) | `likedby:<your name>` plays everything you have reacted to, from Makapix itself (website likes included) |
| 21 | A unique name per panel, for a second unit | S | 5 | Every board is `pixel-jukebox.local` today |
| 22 | Companion app (Flutter) | L | 5 | The REST API is ready for it |
| 30 | Explore Bluetooth (BLE) | M | Later | A learning project. Ideas: Wi-Fi setup from a phone instead of the hotspot, or the companion app finding and controlling the panel without Wi-Fi |

## Slots and effects

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 23 | More visualizers | S each | 5 | Done in 0.30.0: pulse rings, starfield, stereo scope, hi-fi meter, and a fire that follows the beat (`beat_follower.h`). Left: artwork that dances to the beat (passed over for now). A waterfall was tried and dropped: too faint to read on the panel |
| 24 | Built-in effects: falling code, rainbow, Game of Life | S each | 5 | |
| 25 | Per-slot settings, for example clock colours | S-M | 5 | |

## Housekeeping

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 26 | ~~Refresh the README roadmap~~ | S | Done (0.26.0) | The roadmap now points here instead of keeping its own list, and the Development section describes folders rather than every file |
| 27 | CI build on GitHub | S | Deferred | Deferred by choice in the architecture plan |
| 31 | ~~Crash records in diagnostics~~ | S | Done (0.16.0) | `GET /crash`, `tools/decode_crash.py`. One record from an earlier, unarchived build was found: the loader, a null-pointer write while freeing memory. Watch for it again |
| 33 | ~~Memory headroom review~~ | S-M | Done (0.21.0) | TLS memory moved to PSRAM (lowest free 49 -> 97 KB in normal use); audio released when sound mode ends (18 KB back); one permanent loader task instead of a 16 KB stack per artwork; floors of 48 KB free and a 32 KB block in the smoke test. It also found and fixed the crash behind both crash records: a card file read in one task while a TLS request ran in another (see `storage/card.h`) |
| 34 | ~~Start dark at night~~ | S | Done (0.20.2) | At night the panel starts at the night level (dark when off) and skips the test pattern; Wi-Fi setup still shows at day brightness |
