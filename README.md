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
| `include/pins.h` | HUB75 GPIO mapping and panel size |
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

## Makapix API script

```bash
python tools/makapix_player.py
```

The first run registers a new player: it prints a 6-character code to enter at
makapix.club under Settings > Players > Add Device, then stores the player key and
bearer token in `tools/.player_state.json`. It then queries 64x64 GIFs and saves a
few to `tools/downloads/`.

## To do

- Verify the makapix.club TLS certificate (currently `setInsecure()`)
- Preload the next artwork to remove the pause between swaps
- Report views, and MQTT for "send to device" commands from the web app
- Brightness control, Wi-Fi setup portal, audio
