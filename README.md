# Pixel Jukebox

A 64x64 LED matrix that shuffles through pixel art from [Makapix Club](https://makapix.club).

## Hardware

- Waveshare ESP32-S3-RGB-Matrix driver board
- Waveshare RGB-Matrix-P2-64x64 panel (HUB75, 1/32 scan)
- 5V supply for the panel, connected to the panel's own power lead

Pin mapping is in [include/pins.h](include/pins.h).

## Layout

| Path | Purpose |
|------|---------|
| `src/main.cpp` | Main loop: Wi-Fi, registration, artwork queue |
| `src/makapix.cpp` | Makapix player API and stored credentials (NVS) |
| `src/gif_player.cpp` | GIF download and playback, with whole-number scaling |
| `src/display.cpp` | Panel setup and text screens |
| `src/audio.cpp` | Speaker output through the ES8311 codec (used by `speaker-test` only so far) |
| `include/pins.h` | HUB75, I2C and audio GPIO mapping, panel size |
| `include/ca_certs.h` | Root certificates trusted for makapix.club |
| `include/secrets.example.h` | Wi-Fi credentials template; copy to `secrets.h` |
| `tools/makapix_player.py` | PC script that walks the Makapix player API |

## Firmware

1. Install the PlatformIO extension for VS Code.
2. Copy `include/secrets.example.h` to `include/secrets.h` and fill in your 2.4 GHz network.
3. Build and upload, then open the serial monitor:

```bash
pio run -t upload
```

```bash
pio device monitor
```

On first boot the panel shows a 6-character code. Enter it at makapix.club under
Settings > Players > Add Device; the board stores its token and starts playing. It
shuffles every post that is 64 pixels or smaller and has a GIF rendition, showing each
for its dwell time (30 seconds by default).

If an upload fails with "No serial data received", unplug both USB cables, hold BOOT,
plug the "USB" cable back in, release BOOT, then upload again.

### Diagnostic builds

Two extra PlatformIO environments run a check at boot and then carry on as normal.
Upload the regular firmware again afterwards.

```bash
pio run -e colour-check -t upload
```

Shows white at brightness 32, 64 and 128, then colour bars. If white turns yellow or
red as the brightness rises, the 5V supply is sagging; lower `BRIGHTNESS` in
`src/display.cpp` or use a stronger supply.

```bash
pio run -e speaker-test -t upload
```

Lists the devices on the I2C bus, then plays a chime and a steady tone three times.

## Makapix API script

```bash
python tools/makapix_player.py
```

The first run registers a new player: it prints a 6-character code to enter at
makapix.club under Settings > Players > Add Device, then stores the player key and
bearer token in `tools/.player_state.json`. It then queries 64x64 GIFs and saves a
few to `tools/downloads/`.

## To do

- Power the panel from the dedicated 5V input; over USB-C, white tints above brightness 32
- Report views, and MQTT for "send to device" commands from the web app
- Brightness control and a Wi-Fi setup portal
- Use the speaker, microphones and onboard sensors
- Find out why a few posts return a tiny file that is not a GIF (they are skipped)
