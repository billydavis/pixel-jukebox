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
| 4 | **Playsets** | M | 4 | Weighted mixes of channels set up on makapix.club; can include the card |
| 5 | **Live presence and remote commands (MQTT)** | L | 3 | "Online" on Makapix with the artwork showing, and next / previous / show this / switch channel from the website |
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
| 15 | Shorten the 40 s wait before playing offline at boot | S | 4 | Two 20 s attempts to join Wi-Fi when the router is down |
| 28 | **Starter art in the spare flash** | M | 3 | The firmware reserves a 23 MB storage area it never uses. A few dozen built-in pieces would give a new panel something to play before it is on Wi-Fi or registered, and offline without a card |
| 29 | Auto-dim when the room is quiet | S | 4 | The board has no light sensor, so it cannot tell dark; it can tell quiet, from the microphones. The schedule (#10) covers night. A light sensor on the I2C connector would be a hardware project |
| 32 | ~~Photo mode~~ | S | Done (0.17.0) | About 520 Hz instead of 100 so cameras film without bands; restarts to switch. Open: check the picture at the faster clock, and whether normal mode should run at 16 MHz too (200 Hz, same colours) |

## Control page, API and apps

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 16 | **Split the control page into pages** | M | 3 | Decide the layout as features arrive; includes where Restart lives |
| 17 | Upload your own art to the card ("My art") | M | 4 | Needs 18 first: anyone on the Wi-Fi could fill the card |
| 18 | A PIN for the control page and API | M | 3 | No authentication today. p64 has one to learn from |
| 19 | Updates over Wi-Fi (OTA) | M | 3 | No USB cable needed for new firmware. p64 installs from GitHub releases |
| 20 | ~~A "Liked" channel~~ | S | Done (0.18.0) | `likedby:<your name>` plays everything you have reacted to, from Makapix itself (website likes included) |
| 21 | A unique name per panel, for a second unit | S | 5 | Every board is `pixel-jukebox.local` today |
| 22 | Companion app (Flutter) | L | 5 | The REST API is ready for it |
| 30 | Explore Bluetooth (BLE) | M | Later | A learning project. Ideas: Wi-Fi setup from a phone instead of the hotspot, or the companion app finding and controlling the panel without Wi-Fi |

## Slots and effects

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 23 | More visualizers: waterfall, pulse ring, artwork that moves to the beat | S each | 5 | |
| 24 | Built-in effects: falling code, rainbow, Game of Life | S each | 5 | |
| 25 | Per-slot settings, for example clock colours | S-M | 5 | |

## Housekeeping

| # | Item | Size | Priority | Notes |
|---|---|---|---|---|
| 26 | Refresh the README roadmap | S | with the next commit | Lists done items as ideas (full brightness, show a specific post) |
| 27 | CI build on GitHub | S | Deferred | Deferred by choice in the architecture plan |
| 31 | ~~Crash records in diagnostics~~ | S | Done (0.16.0) | `GET /crash`, `tools/decode_crash.py`. One record from an earlier, unarchived build was found: the loader, a null-pointer write while freeing memory. Watch for it again |
| 33 | Memory headroom review | S-M | 3 | Internal RAM is about 108 KB free at rest but dips to about 42-50 KB during each Makapix TLS session (0.20.0). p64 keeps 48 KB as a floor. Look at what sits in internal RAM (task stacks, feed queues of Strings, JSON) and move what can go to PSRAM |
