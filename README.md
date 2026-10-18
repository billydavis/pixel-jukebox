<div align="center">
  <img src="logo.png" alt="Pixel Jukebox" width="120" />

  <h1>Pixel Jukebox</h1>

  <p>A 64x64 LED matrix that shuffles through pixel art from <a href="https://makapix.club">Makapix Club</a>:<br><strong>plug it in, enter a code, and it plays.</strong></p>

  <p>
    <img src="https://img.shields.io/badge/ESP32-S3-E7352C?logo=espressif&logoColor=white" />
    <img src="https://img.shields.io/badge/PlatformIO-Arduino-F5822A?logo=platformio&logoColor=white" />
    <img src="https://img.shields.io/badge/HUB75-64x64-8A2BE2" />
    <img src="https://img.shields.io/badge/Makapix-Club-FF4081" />
    <img src="https://img.shields.io/badge/License-Apache_2.0-3DA639" />
  </p>
</div>

<!-- Photo of the panel playing artwork goes here:
<p align="center">
  <img src="pixel-jukebox-promo.webp" alt="Pixel Jukebox playing artwork" width="800" />
</p>
-->

---

## What it is

Pixel Jukebox is firmware for a Waveshare ESP32-S3 driver board and a 64x64 HUB75 LED panel. It registers itself as a player on Makapix Club, a pixel art community, then pulls artwork from the public feed and shows each piece for about 30 seconds, like a jukebox working through a stack of records.

There is no app and no server of your own to run. The board talks to Makapix directly over Wi-Fi and remembers its registration across restarts.

## Features

| Area | What's included |
|---|---|
| **Slots** | Rotates through screens: by default three artworks, then a clock for 10 seconds. The rotation is editable from the control page and saved on the board |
| **Night schedule** | Between two times, the panel dims the art, shows only the clock, or turns off, fading over a few minutes each way. Moving the brightness or pressing Next at night takes over for an hour |
| **Photo mode** | A switch on the control page redraws the panel about five times as often (about 500 Hz), so phones and cameras film it without dark bands rolling through. Dark shades are less smooth, so it is off by default. The panel restarts to switch |
| **Clock chip** | The board's clock chip keeps the time through restarts, so the clock and view reports have it before Wi-Fi is up. It is set from internet time whenever that answers. A backup battery keeps the time through power cuts too: a non-rechargeable 3 V coin cell (CR2032 or CR1220) on the 2-pin JST-SH 1.0 mm connector J4, pin 1 positive. The board does not charge it |
| **Pausing slots** | An eye on each rotation row pauses a slot without removing it, like hiding a layer in an image editor: it keeps its place, turns and channel and is skipped until shown again. Up to 12 slots |
| **Channels** | Each artwork slot in the rotation can play its own Makapix channel: everything, featured art, your own art, one artist, a hashtag, or the art you have liked. For example two featured pieces, one tagged #cat, one you liked, then the clock |
| **Live on makapix.club** | The panel shows as online on the website with the artwork it is showing, and the website can press Next and Back, send it a particular artwork, or have it play a channel (an artist, a hashtag, featured art). A switch in Settings turns it off |
| **Likes and views** | A heart on the control page likes the artwork on Makapix as the panel's owner, and every artwork shown is reported as a view so artists see how often their work is seen. Both wait for the connection when offline; view reports can be turned off |
| **Offline play** | With a microSD card in the slot, every artwork played is saved to it (up to 1,500 pieces or 1 GB). When Makapix or the Wi-Fi is unreachable, the board shuffles through those copies instead, and goes back to fresh art when the connection returns. No card, no cache; the card is never formatted by the board |
| **Transitions** | Thirteen effects between slots, such as mosaic, melt, dissolve and slide. Random by default, or pick one, or turn them off. They never eat into a slot's time on screen |
| **Button** | BOOT button: short press skips to the next slot, long press holds the current one, a 5-second hold starts Wi-Fi setup |
| **Sound mode** | Switch from the artwork to a visualizer driven by the onboard microphones: spectrum bars, VU meter, oscilloscope or fire. It stays on until you switch back, and scales itself to how loud the room is |
| **Control page** | Open `http://pixel-jukebox.local` on a phone or PC: see the artwork and artist, go back, skip, hold, set brightness |
| **REST API** | Status, skip, hold and brightness over HTTP, found on the network as `pixel-jukebox.local` |
| **Wi-Fi setup** | With no network configured, the board starts its own hotspot and a page for choosing one. No code editing needed |
| **Registration** | Shows a 6-character code on the panel; enter it on makapix.club and the board stores its token |
| **Artwork feed** | Shuffles every post that is 64 pixels or smaller and has a GIF rendition, 25 at a time |
| **Playback** | Animated and still GIFs, each shown for its dwell time, finishing the loop in progress |
| **Scaling** | 16x16 and 32x32 art is scaled by a whole number and centred, so pixels stay sharp |
| **Preloading** | The next artwork downloads in the background while the current one plays |
| **Recovery** | Re-registers if the device is removed on the site; reconnects after a Wi-Fi drop; skips bad files |
| **Updates over Wi-Fi** | Send new firmware from the PC with `python tools/ota_upload.py`, or the Update button in Settings. It is written alongside the running firmware and only used once it checks out; if it then fails to start properly, the panel goes back to the previous firmware by itself. USB still works |
| **PIN** | An optional 4 to 8 digit PIN for the control page and the API, set in Settings. Each browser signs in once and stays signed in, through restarts. Five wrong guesses lock sign-in for 30 seconds. Forgot it? Hold the button for 5 seconds and tick "Also remove the control-page PIN" on the Wi-Fi setup page |
| **Security** | Verifies the makapix.club TLS certificate before sending the device token |
| **Diagnostics** | Separate builds for a colour and power check and a speaker test |

## Hardware

| Part | Notes |
|---|---|
| Waveshare ESP32-S3-RGB-Matrix | Driver board; ESP32-S3 with 32 MB octal flash and 16 MB octal PSRAM |
| Waveshare RGB-Matrix-P2-64x64 | HUB75 panel, 2 mm pitch, 1/32 scan |
| microSD card (optional) | Any size, formatted FAT32 (or FAT16) on a computer first. Cards of 64 GB and up usually come as exFAT, which the board cannot read |
| 5V supply | Waveshare advises 4 A per panel. Make sure the panel's power lead is firmly connected: a poor connection makes white turn yellow or red as brightness rises |

Pin mapping is in [`include/pins.h`](include/pins.h).

## Stack

| Layer | Technology |
|---|---|
| Build | PlatformIO, Arduino framework (arduino-esp32 2.x) |
| Display | ESP32-HUB75-MatrixPanel-DMA, Adafruit GFX |
| Artwork | AnimatedGIF, decoded from RAM (PSRAM) |
| Network | Makapix player API over HTTPS, ArduinoJson; images over plain HTTP from the vault |
| Remote control | Built-in WebServer and mDNS, JSON over HTTP; pages embedded from `web/` at build time |
| Storage | NVS (Preferences) for Wi-Fi details, the player key and token, brightness and the rotation |
| Audio | ES7210 microphone ADC and ES8311 speaker codec over I2C and I2S; a built-in FFT for the visualizers |

## Getting started

**Prerequisites:** VS Code with the PlatformIO extension, and a [Makapix Club](https://makapix.club) account

```sh
git clone https://github.com/billydavis/pixel-jukebox.git
cd pixel-jukebox
pio run -t upload
```

### Wi-Fi setup

On first boot the panel shows "WiFi setup" and starts an open network named `Pixel-Jukebox`.

1. Join `Pixel-Jukebox` on a phone. A setup page opens by itself; if it doesn't, browse to `http://10.10.10.1`.
2. Pick your home network from the list (2.4 GHz only) and enter its password.
3. The panel tries the network. If it joins, the details are saved and it restarts; if not, the page says so within 15 seconds and nothing is saved.

To skip this step, copy `include/secrets.example.h` to `include/secrets.h` and fill in your network before uploading. Details saved through the setup page take priority over that file.

To change networks later, hold the BOOT button for 5 seconds, or use **Change Wi-Fi** on the control page. If the saved network can't be joined at startup, the panel offers setup again by itself, and goes back to retrying the saved network after three minutes with nobody connected. The exception is a board with artwork cached on its card: it plays that instead and keeps retrying in the background, on the basis that the router is more likely down than the password wrong. Hold BOOT for 5 seconds to open setup in that case.

### Makapix registration

Once online, the panel shows a 6-character code. Enter it at makapix.club under **Settings > Players > Add Device**. The board shows "linked!" and starts playing.

### Uploading

The board has two USB-C ports: **Power** feeds the board and the panel, **USB** carries data to the PC.

If an upload fails with "No serial data received", unplug both cables, hold **BOOT**, plug the **USB** cable back in, release **BOOT**, then upload again.

### Time zone

The clock's time zone is `TIMEZONE` in [`include/config.h`](include/config.h) (US Central by default).

### Brightness

Brightness (0-255) is set through the API and remembered across restarts; the starting value is `DEFAULT_BRIGHTNESS` in [`src/display/display.cpp`](src/display/display.cpp). Raise it only as far as white stays white; see the colour check below.

## Control page

Open `http://pixel-jukebox.local` (or the board's IP address) in a browser on the same network; it is laid out for phones first. **Now** shows what is on the panel and who made it, with a heart to like it, Hold, Next, brightness and a Jukebox / Sound switch. Tabs hold the rest: **Rotation** (which slots play, for how long, paused or not, and the transition between them), **Art** (the card's saved art, with a gallery to show or remove pieces), **Night** (the schedule) and **Settings** (photo mode, view reports, Wi-Fi, restart and device details). On a phone the tabs sit at the bottom with a small now-playing bar above each one; on a wide screen now playing stays on the left. On a keyboard, `N` skips, `H` holds and the arrow keys move between tabs. It is a single file, [`web/index.html`](web/index.html), compiled into the firmware, with no external assets.

## REST API

The board serves a small JSON API for remote control, at `http://pixel-jukebox.local/api/v1`. The full contract is in [`docs/jukebox-api.md`](docs/jukebox-api.md).

```sh
curl http://pixel-jukebox.local/api/v1/status
curl -X POST http://pixel-jukebox.local/api/v1/next
```

A Postman collection is in [`postman/`](postman/Pixel-Jukebox-API.postman_collection.json); set its `device_ip` variable to the board's address. It is generated by `tools/make_postman.py`, so add or change requests there and run the script.

It is open to anyone on the same network until a PIN is set (Settings on the control page). With one set, scripts send it in an `X-Jukebox-Pin` header; see [The PIN](docs/jukebox-api.md#the-pin).

## Diagnostic builds

Four extra PlatformIO environments run a check at boot and then carry on as normal. Upload the regular firmware again afterwards.

```sh
pio run -e colour-check -t upload
```

Shows white at brightness 32, 64 and 128, then colour bars. If white turns yellow or red as the brightness rises, the 5V supply is sagging: check the power wiring first, then the supply's rating.

```sh
pio run -e speaker-test -t upload
```

Lists the devices on the I2C bus, then plays a chime and a steady tone three times.

```sh
pio run -e mic-test -t upload
```

Plays a short tone and checks both microphones hear it, then shows each microphone's level as a bar for a minute.

```sh
pio run -e motion-test -t upload
```

Finds the motion sensor, then for two minutes shows the pull of gravity on each axis, a bar for how hard the last knock was (the amber mark is the default tap threshold, 1.5 g), and how many single and double taps were detected. The border flashes green for a single tap and blue for a double. Each knock is also logged on the serial monitor.

## Crash records

If the board crashes, or a watchdog restarts it, it keeps a record of what happened in flash until it is cleared. `GET /api/v1/crash` shows it, and from a PC:

```sh
python tools/decode_crash.py
```

prints the task, the cause and the backtrace as function names and source lines. Every build keeps a copy of its ELF in `.pio/elf-archive/` for this, so a crash can be decoded even after newer firmware has been built. `--dump FILE` also saves the full record for ESP-IDF's `espcoredump.py`. Clear it with `curl -X DELETE http://pixel-jukebox.local/api/v1/crash`.

## Makapix API script

[`tools/makapix_player.py`](tools/makapix_player.py) walks the same player API from a PC, using only the Python standard library. It is useful for trying queries before changing the firmware.

```sh
python tools/makapix_player.py                 # register (first run), query, download
python tools/makapix_player.py --channel hashtag --hashtag landscape --download 5
python tools/makapix_player.py --reset         # forget the stored player
```

The first run registers a second player on your account, named `pixel-jukebox-pc-tool` so it is easy to tell from the panel. Its key and token are kept in `tools/.player_state.json`, and downloads go to `tools/downloads/`; both are git-ignored.

## Development

The firmware is grouped by area, one folder each. Every folder holds a handful of `.cpp`/`.h` pairs named for what they do, and each header opens with a comment saying what that module is for, so start there.

| Folder | What is in it |
|---|---|
| `src/` | `main.cpp`: start-up, running each slot, handling commands |
| `src/app/` | What the panel is doing: the rotation, modes, transitions, night schedule, clock chip, button and command queue, firmware updates, crash records and diagnostics |
| `src/slots/` | The things that can be on screen: artwork, clock, visualizers. Each implements `slot.h` |
| `src/display/` | Drawing to the panel, and GIF playback |
| `src/audio/` | Speaker, microphones and the sound analysis behind the visualizers |
| `src/input/` | Onboard sensors: motion, and the case temperature and humidity |
| `src/network/` | Wi-Fi and its setup hotspot, the API and its PIN, the Makapix client, the live connection to makapix.club, likes and view reports |
| `src/storage/` | The microSD card and the artwork saved on it |
| `lib/jukebox_core/` | Logic with no hardware in it (rules, maths, parsing), so it can be unit tested on a PC |
| `test/test_core/` | The unit tests for `lib/jukebox_core` |
| `web/` | The control page and Wi-Fi setup pages as plain HTML and CSS, embedded into the firmware at build time |
| `include/` | Board pins, version and settings, trusted certificates, the optional `secrets.h` |
| `tools/` | PC-side scripts: the tests and checks below, crash decoding, the Postman generator |
| `docs/` | The API contract, the architecture plan and the [backlog](docs/backlog.md) |

A few conventions:

- `#include` paths are written from `src/`, for example `#include "display/display.h"`.
- Anything that can be tested without the board goes in `lib/jukebox_core/`, with tests.
- Card access and Makapix requests share one lock (`CardLock` in `src/storage/card.h`); new code that touches either must take it.
- A new API route needs its entry in `docs/jukebox-api.md` and `tools/make_postman.py`; `tools/check_api_docs.py` fails until both have it.
- Code adapted from p64 keeps its copyright and licence notice (see Acknowledgements).

```sh
pio run                          # build
pio run -t upload                # build and flash over USB
python tools/ota_upload.py       # flash the last build over Wi-Fi
pio device monitor               # serial log at 115200 baud
```

## Testing

Four checks, from fastest to most thorough.

```sh
python tools/check_api_docs.py
```

Compares the routes in `src/network/api.cpp` with the API document and the Postman collection, and checks the collection matches its generator. Needs no board.

```sh
python tools/check_web.py
```

Checks the pages in `web/` for duplicate element IDs, script references to IDs that do not exist, anything loaded from outside the board, placeholders the firmware never fills in, and colour tokens that differ between the control page and the setup pages. Needs no board.

```sh
powershell -File tools/run_unit_tests.ps1
```

Runs the unit tests in `test/` for the hardware-free code in `lib/jukebox_core`. It compiles them on the PC with the Visual Studio C++ compiler, so it takes a few seconds and needs no board.

The same tests also run on the board itself, built by the ESP32 compiler, with `pio test -e waveshare-s3-rgb-matrix`. That replaces the firmware, so upload the regular firmware again afterwards.

```sh
python tools/api_smoke_test.py 192.168.4.105
```

Exercises every API endpoint against a running board except Wi-Fi setup, including the rejections and the PIN (set, tried every way, locked out, removed), and puts the settings back as they were. Takes about two minutes. With a PIN set on the panel, put it in the `JUKEBOX_PIN` environment variable first.

Behaviour that depends on the hardware is covered by the diagnostic builds above.

## Roadmap

Everything planned, with rough sizes and priorities, is in [docs/backlog.md](docs/backlog.md). Next up: firmware updates over Wi-Fi, and live presence and remote commands from the Makapix site (MQTT). Waiting on the case: tap to like and auto-rotation, using the motion sensor.

## Acknowledgements

Pixel Jukebox has two inspirations. The first is [p64](https://github.com/fabkury/p64) by fabkury, who also runs Makapix Club. It is a desk display built on exactly the same Waveshare board and panel, and it is the reference this project turns to for feature ideas and for solutions to problems that hardware throws up. Pixel Jukebox is not a clone of p64 and is written independently, but because the two share their hardware, many of their choices will look alike.

p64 is licensed under the Apache License 2.0. Where Pixel Jukebox adapts code from it rather than just an idea, the file says so and keeps p64's copyright and licence notice, as that licence requires.

The second is [Pixelix](https://github.com/BlueAndi/Pixelix) by BlueAndi, a far more complete LED matrix firmware with a plugin system, web interface and its own Makapix player. It is a source of ideas only; Pixel Jukebox shares no code with Pixelix.

Thanks also to [Makapix Club](https://makapix.club) for the open player API and its documentation, and to the authors of the libraries listed under Stack.

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.

Versions up to and including firmware 0.10.0 (commit `55bd0d2`) were released under the MIT License, and copies taken under those terms remain under them.
