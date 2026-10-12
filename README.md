<div align="center">
  <img src="logo.png" alt="Pixel Jukebox" width="120" />

  <h1>Pixel Jukebox</h1>

  <p>A 64x64 LED matrix that shuffles through pixel art from <a href="https://makapix.club">Makapix Club</a>:<br><strong>plug it in, enter a code, and it plays.</strong></p>

  <p>
    <img src="https://img.shields.io/badge/ESP32-S3-E7352C?logo=espressif&logoColor=white" />
    <img src="https://img.shields.io/badge/PlatformIO-Arduino-F5822A?logo=platformio&logoColor=white" />
    <img src="https://img.shields.io/badge/HUB75-64x64-8A2BE2" />
    <img src="https://img.shields.io/badge/Makapix-Club-FF4081" />
    <img src="https://img.shields.io/badge/License-MIT-3DA639" />
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
| **Button** | BOOT button: short press skips to the next slot, long press holds the current one, a 5-second hold starts Wi-Fi setup |
| **Sound mode** | Switch from the artwork to a visualizer driven by the onboard microphones: spectrum bars, VU meter, oscilloscope or fire. It stays on until you switch back, and scales itself to how loud the room is |
| **Control page** | Open `http://pixel-jukebox.local` on a phone or PC: see the artwork and artist, skip, hold, set brightness |
| **REST API** | Status, skip, hold and brightness over HTTP, found on the network as `pixel-jukebox.local` |
| **Wi-Fi setup** | With no network configured, the board starts its own hotspot and a page for choosing one. No code editing needed |
| **Registration** | Shows a 6-character code on the panel; enter it on makapix.club and the board stores its token |
| **Artwork feed** | Shuffles every post that is 64 pixels or smaller and has a GIF rendition, 25 at a time |
| **Playback** | Animated and still GIFs, each shown for its dwell time, finishing the loop in progress |
| **Scaling** | 16x16 and 32x32 art is scaled by a whole number and centred, so pixels stay sharp |
| **Preloading** | The next artwork downloads in the background while the current one plays |
| **Recovery** | Re-registers if the device is removed on the site; reconnects after a Wi-Fi drop; skips bad files |
| **Security** | Verifies the makapix.club TLS certificate before sending the device token |
| **Diagnostics** | Separate builds for a colour and power check and a speaker test |

## Hardware

| Part | Notes |
|---|---|
| Waveshare ESP32-S3-RGB-Matrix | Driver board; ESP32-S3 with 32 MB octal flash and 16 MB octal PSRAM |
| Waveshare RGB-Matrix-P2-64x64 | HUB75 panel, 2 mm pitch, 1/32 scan |
| 5V supply | Waveshare advises 4 A per panel; over the board's USB-C power port, white tints above brightness 32 |

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

To change networks later, hold the BOOT button for 5 seconds, or use **Change Wi-Fi** on the control page. If the saved network can't be joined at startup, the panel offers setup again by itself, and goes back to retrying the saved network after three minutes with nobody connected.

### Makapix registration

Once online, the panel shows a 6-character code. Enter it at makapix.club under **Settings > Players > Add Device**. The board shows "linked!" and starts playing.

### Uploading

The board has two USB-C ports: **Power** feeds the board and the panel, **USB** carries data to the PC.

If an upload fails with "No serial data received", unplug both cables, hold **BOOT**, plug the **USB** cable back in, release **BOOT**, then upload again.

### Time zone

The clock's time zone is `TIMEZONE` in [`include/config.h`](include/config.h) (US Central by default).

### Brightness

Brightness (0-255) is set through the API and remembered across restarts; the starting value is `DEFAULT_BRIGHTNESS` in [`src/display.cpp`](src/display.cpp). Raise it only as far as white stays white; see the colour check below.

## Control page

Open `http://pixel-jukebox.local` (or the board's IP address) in a browser on the same network. The page shows what is on the panel and who made it, with Hold, Next and a brightness slider, a Jukebox / Sound switch for choosing a visualizer, and a Rotation section for choosing which slots play and for how long. On a keyboard, `N` skips and `H` holds. It is a single file, [`web/index.html`](web/index.html), compiled into the firmware, with no external assets.

## REST API

The board serves a small JSON API for remote control, at `http://pixel-jukebox.local/api/v1`. The full contract is in [`docs/jukebox-api.md`](docs/jukebox-api.md).

```sh
curl http://pixel-jukebox.local/api/v1/status
curl -X POST http://pixel-jukebox.local/api/v1/next
```

A Postman collection is in [`postman/`](postman/Pixel-Jukebox-API.postman_collection.json); set its `device_ip` variable to the board's address.

There is no authentication: anyone on the same network can call it.

## Diagnostic builds

Three extra PlatformIO environments run a check at boot and then carry on as normal. Upload the regular firmware again afterwards.

```sh
pio run -e colour-check -t upload
```

Shows white at brightness 32, 64 and 128, then colour bars. If white turns yellow or red as the brightness rises, the 5V supply is sagging: lower the brightness or use a stronger supply.

```sh
pio run -e speaker-test -t upload
```

Lists the devices on the I2C bus, then plays a chime and a steady tone three times.

```sh
pio run -e mic-test -t upload
```

Plays a short tone and checks both microphones hear it, then shows each microphone's level as a bar for a minute.

## Makapix API script

[`tools/makapix_player.py`](tools/makapix_player.py) walks the same player API from a PC, using only the Python standard library. It is useful for trying queries before changing the firmware.

```sh
python tools/makapix_player.py                 # register (first run), query, download
python tools/makapix_player.py --channel hashtag --hashtag landscape --download 5
python tools/makapix_player.py --reset         # forget the stored player
```

The first run registers a second player on your account. Its key and token are kept in `tools/.player_state.json`, and downloads go to `tools/downloads/`; both are git-ignored.

## Development

| Path | Purpose |
|---|---|
| `src/main.cpp` | Board start-up, running each slot and command handling |
| `src/connection.cpp` | Joining Wi-Fi at boot and rejoining after a drop |
| `src/diagnostics.cpp` | The speaker and microphone checks for the diagnostic builds |
| `src/rotation.cpp` | The slot types and the saved rotation |
| `src/slot.h` | Interface every slot implements |
| `src/slot_makapix.cpp` | Artwork slot: registration, background loader, playback |
| `src/slot_clock.cpp` | Clock slot (network time) |
| `src/commands.cpp` | Command queue and the BOOT button |
| `src/wifi_setup.cpp` | Wi-Fi credentials and the setup hotspot |
| `src/api.cpp` | HTTP server and mDNS |
| `web/` | The control page and the Wi-Fi setup pages, as ordinary HTML and CSS files |
| `src/web_assets.h` | Names for the `web/` files once they are embedded in the firmware |
| `src/telemetry.cpp` | Memory and task headroom for `GET /diagnostics` |
| `src/status.cpp` | Shared "now showing" state read by the API |
| `src/makapix.cpp` | Makapix player API and stored credentials |
| `src/gif_player.cpp` | GIF download and frame-by-frame playback, with whole-number scaling |
| `src/display.cpp` | Panel setup, text screens, colour check |
| `src/mode.cpp` | Rotation or sound mode, and the chosen visualizer |
| `src/slot_visualizers.cpp` | Spectrum bars, VU meter, oscilloscope and fire |
| `src/audio_analysis.cpp` | Frequency analysis and automatic level scaling for the visualizers |
| `src/audio.cpp` | Speaker output (ES8311) and microphone input (ES7210) |
| `lib/jukebox_core/` | Hardware-free logic, covered by the unit tests |
| `test/test_core/` | Unit tests |
| `tools/run_unit_tests.ps1` | Builds and runs the unit tests on a Windows PC |
| `tools/api_smoke_test.py` | End-to-end check of the API against a board |
| `tools/check_api_docs.py` | Checks the API document and Postman collection against the firmware |
| `tools/check_web.py` | Checks the pages in `web/` for mistakes a browser would not report |
| `include/pins.h` | HUB75, I2C and audio GPIO mapping, panel size |
| `include/ca_certs.h` | Root certificates trusted for makapix.club |
| `include/secrets.example.h` | Optional compiled-in Wi-Fi credentials |

```sh
pio run                 # build
pio run -t upload       # build and flash
pio device monitor      # serial log at 115200 baud
```

## Testing

Four checks, from fastest to most thorough.

```sh
python tools/check_api_docs.py
```

Compares the routes in `src/api.cpp` with the API document and the Postman collection. Needs no board.

```sh
python tools/check_web.py
```

Checks the pages in `web/` for duplicate element IDs, script references to IDs that do not exist, anything loaded from outside the board, placeholders the firmware never fills in, and colour tokens that differ between the control page and the setup pages. Needs no board.

```sh
powershell -File tools/run_unit_tests.ps1
```

Runs the unit tests in `test/` for the hardware-free code in `lib/jukebox_core` (small-GIF padding, HTML escaping, rotation rules, the visualizers' frequency maths). It compiles them on the PC with the Visual Studio C++ compiler, so it takes a few seconds and needs no board.

The same tests also run on the board itself, built by the ESP32 compiler, with `pio test -e waveshare-s3-rgb-matrix`. That replaces the firmware, so upload the regular firmware again afterwards.

```sh
python tools/api_smoke_test.py 192.168.4.105
```

Exercises every API endpoint against a running board except Wi-Fi setup, including the rejections, and puts brightness, mode and rotation back as they were. Takes about a minute.

Behaviour that depends on the hardware is covered by the diagnostic builds above.

## Roadmap

Working today: registration, shuffled playback with preloading, a slot rotation with a clock, a sound mode with four visualizers, button, web and REST control, certificate checking, and the diagnostic builds.

Ideas for later:

- Power the panel from the dedicated 5V input for full brightness
- Report views, and MQTT for "send to device" commands from the Makapix site
- A brightness schedule for day and night
- Use the speaker and the onboard sensors
- More of the API: show a specific post, sensors, per-slot settings
- A companion app (Flutter)

Ideas borrowed from [Pixelix](https://github.com/BlueAndi/Pixelix), using parts already on the board:

- More slot types
- Keep time across restarts with the onboard clock chip
- Room temperature and humidity (onboard sensor), and a weather screen
- More visualizers (waterfall, pulse ring, artwork that moves to the beat)
- Built-in effects such as falling code, rainbow and Game of Life
- A web page for settings, and firmware updates over Wi-Fi

## Acknowledgements

Pixel Jukebox is inspired by [Pixelix](https://github.com/BlueAndi/Pixelix) by BlueAndi, a far more complete LED matrix firmware with a plugin system, web interface and its own Makapix player. Seeing Makapix art on a Pixelix display is what started this project. Pixel Jukebox is written from scratch as a learning project and shares no code with Pixelix.

Thanks also to [Makapix Club](https://makapix.club) for the open player API and its documentation, and to the authors of the libraries listed under Stack.

## License

MIT License. See [LICENSE](LICENSE) for details.
