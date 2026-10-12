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
| 2 | **CI build.** GitHub Actions workflow building the regular firmware and the diagnostic builds, without `secrets.h` | Not started |
| 3 | **Tests.** PC-side unit tests for the pure functions listed above; a smoke-test script that exercises the API on a real board; the route-consistency check | Not started |
| 4 | **Web assets.** Control and setup pages as `.html` files embedded at build time; a duplicate-ID check in the tests | Not started |
| 5 | **Tidy `main.cpp`.** Move the diagnostics and the Wi-Fi lifecycle into their own modules | Not started |
| 6 | **Regroup `src/`.** One mechanical commit | Not started |
| 7 | **Metadata.** Give the PC tool its own identity; stop hard-coding the firmware version in the API example; commit the Postman generator | Not started |

Steps 1 and 2 are small and independent. Steps 5 and 6 are the only ones that
touch many files, which is why they follow the safety net.

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
