# Pixel Jukebox Architecture Review

## Overall assessment

The project already has sensible foundations: responsibilities are mostly
separated into paired modules, the `Slot` interface gives playback modes a
common lifecycle, and artwork preloading keeps network work off the display
loop. A large rewrite or a plugin framework is not warranted at this stage.
The highest-value improvements are clearer grouping, a thinner runtime
coordinator, and measurement of memory and task headroom.

## Prioritized recommendations

### 1. Group source files by domain as the project grows

The flat `src/` layout is still manageable, but network, audio, display, and
slot implementations are already mixed together. Consider gradually grouping
related `.cpp` and `.h` pairs into areas such as `app`, `network`, `display`,
`audio`, and `slots`. Keep project-wide board settings in `include/`, and keep
implementation-specific headers beside their source.

This should improve navigation without adding abstraction layers. The current
organization is documented in the [README](../README.md).

### 2. Reduce responsibilities in the main runtime coordinator

[`src/main.cpp`](../src/main.cpp) handles startup, Wi-Fi recovery, slot
dispatch, command processing, and several diagnostic routines. Extracting the
app/slot-running loop and Wi-Fi lifecycle into focused modules would make the
central flow easier to follow and test.

Keep board bring-up and top-level coordination in `main.cpp`; avoid splitting
it into many tiny helpers without a clear ownership boundary.

### 3. Measure memory pressure before optimizing the image pipeline

[`src/slot_makapix.cpp`](../src/slot_makapix.cpp) preloads the next image while
the current image remains in memory. Since
[`src/gif_player.cpp`](../src/gif_player.cpp) permits GIFs up to 4 MiB, the
image buffers alone can total roughly 8 MiB, alongside display and other
allocations.

Add lightweight telemetry for free and largest contiguous PSRAM/heap blocks,
and track task stack high-water marks. If measurements show pressure, make the
image-size limit account for available memory or consider a file-backed or
streaming strategy.

### 4. Preserve the offline, self-contained UI while keeping it maintainable

[`src/web_page.h`](../src/web_page.h) embeds the control page's HTML, CSS, and
JavaScript in one source file. That fits the product requirement that the page
work without external assets, but the file is becoming one of the larger
source files.

If UI work accelerates, keep authored assets separate and generate or embed
them during the build. Do not turn them into runtime downloads or add a
web-server dependency.

### 5. Add focused validation around pure logic and firmware builds

The project currently has no tracked test directory or CI workflow. Start
with host-side tests for logic that can be separated from hardware, such as
rotation validation and API request validation, and a CI build of the normal
and diagnostic PlatformIO environments.

This would make future file moves and refactors safer. Keep
hardware-dependent behavior covered by the existing diagnostic builds and
document how to run them.

### 6. Reduce duplicated project metadata where practical

Firmware version information appears in more than one place, including
[`include/config.h`](../include/config.h) and
[`tools/makapix_player.py`](../tools/makapix_player.py). A small source of
truth or consistency check would prevent those values from drifting.

Likewise, the API contract is represented in documentation and a Postman
collection. A lightweight validation step could help keep examples aligned
with [`docs/jukebox-api.md`](jukebox-api.md).

## Bottom line

Prioritize incremental domain grouping, simplify the main control flow, and
establish memory and build measurements. The current architecture is a good
base; broad rewrites or speculative performance work are unlikely to pay off
until measurements and tests reveal a specific need.
