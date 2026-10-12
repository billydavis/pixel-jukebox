# Architecture plan

A response to [architecture-review.md](architecture-review.md): what will be
done, in what order, and where the plan departs from the review and why.

## Position on each recommendation

| # | Recommendation | Position |
|---|---|---|
| 1 | Group source files by domain | Agreed, as one mechanical commit, done last |
| 2 | Reduce responsibilities in `main.cpp` | Agreed for diagnostics and the Wi-Fi lifecycle; not for testability |
| 3 | Measure memory before optimizing | Agreed, and moved to first |
| 4 | Keep the embedded UI maintainable | Agreed, and done now rather than "if UI work accelerates" |
| 5 | Tests and CI | Agreed on CI; different first test targets |
| 6 | Reduce duplicated metadata | Agreed on the drift; different fix for the version number |

## Departures from the review

**File moves happen once, not gradually (1).** With 32 files in `src/`, a
gradual regroup leaves half-finished states and rewrites the same `#include`
lines more than once. One commit with no behaviour change, after CI and tests
exist to vouch for it, is cheaper and safer. Fewer folders than the five
proposed are likely enough.

**A thinner `main.cpp` is for readability, not testing (2).** The slot runner
depends on the display and FreeRTOS, so running it on a PC would need mocks
larger than the code under test. What is worth moving out are the diagnostic
routines (about 80 of 312 lines) and the Wi-Fi lifecycle. `main.cpp` is not the
largest source file; `wifi_setup.cpp` is.

**Different first tests (5).** API request validation is bound to the web
server, so it is tested against a real board with a script, not on a PC with a
mock server. PC-side unit tests go first to pure functions with a bug history
or a security role:

- small-GIF padding (`gif_player.cpp`), which fixed a real decoder failure
- HTML escaping on the Wi-Fi setup page (`wifi_setup.cpp`), which handles
  untrusted network names
- band mapping and scaling (`audio_analysis.cpp`)
- rotation validation (`rotation.cpp`), as the review suggests

**The version numbers should not be synced (6).** `tools/makapix_player.py`
reports `0.1.0` while the firmware is at `0.8.0`, but the script is a PC tool
that registers with Makapix under the firmware's identity. It should identify
as itself, with its own name and version. For the API contract, a short check
that every route in `api.cpp` appears in both the API document and the Postman
collection covers the real risk; a full document validator does not earn its
keep.

## Not in the review

- **PSRAM has never been confirmed as in use.** It is configured, but nothing
  checks it at run time. If it is not active, the 4 MB image limit cannot be
  met and large images fail silently. Step 1 settles this.
- **The Postman collection has no generator in the repository.** It has been
  produced by throwaway scripts and cannot be regenerated.
- **A duplicate element ID reached the control page** and was only caught by
  manual testing. Real `.html` files and a simple check would catch that class
  of mistake.

## Steps

| Step | Work | Status |
|---|---|---|
| 1 | **Measure.** Report heap, PSRAM and task stack headroom from the board; run a full rotation and sound mode; decide whether the image limit needs to change | Done: no change needed, see below |
| 2 | **CI build.** GitHub Actions workflow building the regular firmware and the diagnostic builds, without `secrets.h` | Deferred, see below |
| 3 | **Tests.** Unit tests for the pure functions listed above; a smoke-test script that exercises the API on a real board; the route-consistency check | Done, see below |
| 4 | **Web assets.** Control and setup pages as `.html` files embedded at build time; a duplicate-ID check in the tests | Done, see below |
| 5 | **Tidy `main.cpp`.** Move the diagnostics and the Wi-Fi lifecycle into their own modules | Done, see below |
| 6 | **Regroup `src/`.** One mechanical commit | Not started |
| 7 | **Metadata.** Give the PC tool its own identity; stop hard-coding the firmware version in the API example; commit the Postman generator | Not started |

Steps 5 and 6 are the only ones that touch many files, which is why they
follow the tests.

**Step 2 is deferred.** This is a one-person project built locally on every
change, so an automated build is not needed yet. Until it exists, its job is
done by hand: before any commit that moves or restructures code, build all
four environments, and the regular one once more with `include/secrets.h`
moved aside.

```sh
pio run -e waveshare-s3-rgb-matrix -e colour-check -e speaker-test -e mic-test
```

Reasons to revisit: a second contributor, a library update that breaks a
fresh install, or a diagnostic build found broken when it was needed.

## Step 1 results

Measured on the board with `GET /api/v1/diagnostics` (`src/telemetry.cpp`),
firmware 0.8.0, over about nine minutes: 68 artworks downloaded by skipping
every four seconds, each visualizer run in sound mode, and roughly 1,800 API
calls alongside, with no failed calls and no restart.

| Resource | Total | Lowest free seen | Verdict |
|---|---|---|---|
| PSRAM | 16.8 MB | 16.5 MB | Present and in use. Peak use about 0.3 MB |
| Internal heap | about 155 KB free at idle | 61.7 KB | Comfortable. Largest free block 82 KB |
| `display` task stack | 8,192 bytes | 5,864 | 72% unused |
| `api` task stack | 8,192 bytes | 5,816 | 71% unused |
| `loader` task stack | 16,384 bytes | 10,356 | 63% unused |
| `button` task stack | 2,048 bytes | 1,252 | 61% unused |

Findings:

- **PSRAM works.** The open question is closed: image buffers do go to PSRAM.
- **The 4 MB image limit is safe.** The review's worst case, two 4 MB images
  held at once, would use 8 MB and still leave more than 8 MB free.
- **The limit is far from being reached in practice.** The largest of the 68
  artworks was 188 KB.
- **No leak.** Free heap dropped by about 19 KB the first time sound mode ran,
  which is the audio driver's one-time buffers, then held at 135.9 KB across
  three further rounds.
- **The lowest heap figure comes from TLS.** It occurs while the feed is being
  fetched over HTTPS, and recovers afterwards.

Decisions:

- No change to the image limit, and no file-backed or streaming image path.
- No change to task stack sizes. They could be reduced, but internal memory is
  not short, so there is nothing to gain.
- `GET /diagnostics` stays in the firmware, so these figures can be re-read
  after any change that affects memory.

This was a nine-minute test, not a soak. A leak slower than about a kilobyte a
minute would not have shown. Reading `/diagnostics` after a day of normal use
would settle that.

## Step 3 results

Three checks now exist, described in the README's Testing section.

| Check | Covers | Result |
|---|---|---|
| `tools/run_unit_tests.ps1`, or `pio test` on the board | 22 unit tests for `lib/jukebox_core`: small-GIF padding, HTML escaping, rotation rules, FFT band mapping and scaling | All pass |
| `tools/api_smoke_test.py` | 49 checks across every endpoint except Wi-Fi setup, including each rejection, against a running board | All pass |
| `tools/check_api_docs.py` | The 12 routes in `src/api.cpp` against the API document and the Postman collection | In agreement |

What changed in the code to make this possible:

- The four pure functions moved out of `src/` into `lib/jukebox_core`, which
  includes nothing from Arduino or the hardware. The firmware calls them from
  where the code used to be; behaviour is unchanged.
- The unit tests were shown to bite: deliberately corrupting the GIF padding
  made one test fail, and restoring it made all pass again.

The unit tests run in two places from one source file. On the PC,
`tools/run_unit_tests.ps1` compiles them with the Visual Studio C++ compiler in
a few seconds, with no board involved; this is the everyday way to run them.
On the board, `pio test` builds them with the ESP32 compiler, which confirms
the same results on the real target but replaces the firmware until it is
uploaded again. PlatformIO's own PC test mode was not used because it requires
GCC, which is not installed.

Not covered by any of the three: the display, audio capture, GIF decoding on
real artwork, Makapix registration and the Wi-Fi setup hotspot. Those remain
manual, using the diagnostic builds and a phone.

## Step 4 results

The pages now live in `web/` as ordinary files and are compiled into the
firmware by PlatformIO's `board_build.embed_txtfiles`, which needs no extra
build script or dependency. `src/web_page.h` and `src/setup_page.h` are gone.

| File | Purpose | Size |
|---|---|---|
| `web/index.html` | Control page, self-contained | 14.7 KB |
| `web/setup.html` | Wi-Fi setup form | 1.6 KB |
| `web/setup_check.html` | "Checking the password" | 0.6 KB |
| `web/setup_done.html` | "Connected" | 0.5 KB |
| `web/setup.css` | Styles shared by the three setup pages | 2.3 KB |

Verified:

- The board serves `/` byte for byte identical to `web/index.html`.
- The API smoke test passes unchanged (49 of 49).
- All four build environments compile, and the regular one without `secrets.h`.
- `tools/check_web.py` passes, and was shown to catch the earlier bug:
  reintroducing the duplicate `id="art"` made it report both the duplicate and
  the script reference it broke.

Two things changed beyond a straight move:

- **The setup pages share one stylesheet.** Their styles were a C macro pasted
  into three pages; they are now `web/setup.css`, which the hotspot serves at
  `/setup.css`. This is still self-contained: it comes from the board.
- **The control page stays a single file.** Splitting its CSS and JavaScript
  out would mean three requests to a small single-threaded server for no gain
  in maintainability at its current size.

The colour tokens exist twice, in `index.html` and `setup.css`. Removing that
would mean the control page loading a second file, so `check_web.py` compares
the two and fails if they differ.

The setup hotspot, which now makes a second request for its stylesheet, was
checked by hand from a phone and renders correctly.

## Step 5 results

`src/main.cpp` went from 314 lines to 167. It now holds board start-up, the
loop that picks and runs a slot, and command handling, and nothing else.

| Moved to | What | Lines |
|---|---|---|
| `src/connection.cpp` | Joining Wi-Fi at boot, announcing the connection, rejoining after a drop, entering setup | 67 |
| `src/diagnostics.cpp` | The speaker and microphone checks, behind one `runDiagnostics()` call | 99 |
| `src/display.cpp` | The three status colours, which each file had been defining for itself | 3 |

The code was moved, not rewritten; the only edits are names. Verified after
the move: all four environments build, as does the regular one without
`secrets.h`; the unit tests, web check and route check pass; the API smoke
test passes 49 of 49 against the board; and the `mic-test` build was flashed
and its self-check passed, confirming the diagnostics still run from their new
home.

The slot runner stayed in `main.cpp`, as planned. It is the file's main job.

