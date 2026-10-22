# Jukebox API specification

The REST control surface of the Pixel Jukebox firmware. Any client on the same
network can use it: a web page, a companion app, a script.

Status: **implemented and verified on real hardware.** Source: `src/network/api.cpp`.

## Base path

```
/api/v1
```

Versioned from the start so the schema can change later without breaking
clients. The server listens on port 80.

## Finding the device

The board announces itself over mDNS:

- Hostname: `pixel-jukebox.local` (`MDNS_HOSTNAME` in `include/config.h`)
- Service: `_http._tcp`, port 80
- TXT record: `jukebox=/api/v1`

A client scanning for `_http._tcp` can use the `jukebox` TXT key to tell a
jukebox from any other web server, and its value as the API base path.
`GET /api/v1` returns `{"name":"Pixel Jukebox","api":"/api/v1"}`, so a client can
confirm what it has found.

`GET /` serves the built-in control page (`web/index.html`), which is itself a
client of this API.

## Conventions

- Requests and responses are JSON (`Content-Type: application/json`).
- Errors are `{"error": "<message>"}` with a 4xx or 5xx status.
- Open by default. With a PIN set, every route but the base path and the
  sign-in routes needs a session or the PIN; see [The PIN](#the-pin).
- No CORS headers are sent, so a web page served from elsewhere cannot call it
  from a browser.

### Commands are queued

`POST /next`, `POST /hold` and `PUT /brightness` do not act directly. They put a
command on a queue that the display loop reads between frames, the same queue
the BOOT button uses. The reply is `202 Accepted` and names what was queued:

```json
{ "queued": "next" }
```

The change normally takes effect within a few milliseconds; read `GET /status`
to see the result. For the first few seconds after power-on, while the first
artwork loads, a queued command is delayed until playback starts, and `next`
or `hold` may be dropped because there is no slot yet for it to act on. If the queue is full
the reply is `503` and nothing is queued.

The same goes for `POST /card/art/{postId}/show`, `POST /restart`,
`POST /wifi/setup`, and `PUT /panel` when it changes the setting: all answer
`202`, and the last three before the board restarts. Every other change takes
effect before the reply, which is `200` with the new state.

## Endpoints

| Method | Path | Purpose |
|---|---|---|
| `GET` | (the base path, `/api/v1`) | Device name and API base path |
| `GET` | `/status` | What the panel is showing, plus device information |
| `GET` | `/diagnostics` | Memory and task headroom |
| `GET` | `/crash` | The last crash, if one is stored |
| `GET` | `/crash/dump` | The raw crash record, for espcoredump.py |
| `DELETE` | `/crash` | Erase the crash record |
| `GET` | `/likes` | The artworks liked from this panel |
| `PUT` | `/likes/{postId}` | Like an artwork on Makapix |
| `DELETE` | `/likes/{postId}` | Take the like back |
| `GET` | `/views` | Whether views are reported to Makapix |
| `PUT` | `/views` | Turn view reports on or off |
| `GET` | `/night` | The night schedule |
| `PUT` | `/night` | Change the night schedule |
| `GET` | `/panel` | Whether photo mode is on, and the refresh rate |
| `PUT` | `/panel` | Turn photo mode on or off |
| `GET` | `/card` | The microSD card and the offline artwork on it |
| `GET` | `/card/art` | The artworks saved on the card, a page at a time |
| `GET` | `/card/art/{postId}.gif` | One saved artwork's GIF |
| `POST` | `/card/art/{postId}/show` | Show a saved artwork now |
| `DELETE` | `/card/art/{postId}` | Delete a saved artwork from the card |
| `DELETE` | `/card/art` | Delete every saved artwork |
| `POST` | `/restart` | Restart the board |
| `POST` | `/reset` | Reset every setting but Wi-Fi, then restart |
| `POST` | `/next` | Skip to the next slot |
| `POST` | `/back` | Show the artwork before the current one |
| `POST` | `/show/{postId}` | Show any Makapix post for one turn |
| `GET` | `/channel` | The channel playing in place of the rotation's, if any |
| `PUT` | `/channel` | Play one channel in every artwork slot, for now |
| `DELETE` | `/channel` | Go back to the rotation's own channels |
| `GET` | `/live` | The live connection to makapix.club |
| `GET` | `/test` | The test patterns, and whether one is showing |
| `PUT` | `/test` | Show a test pattern |
| `DELETE` | `/test` | Back to art from a test pattern |
| `PUT` | `/live` | Turn the live connection on or off |
| `POST` | `/hold` | Hold or release the current slot |
| `GET` | `/brightness` | Read the brightness |
| `PUT` | `/brightness` | Set the brightness |
| `GET` | `/mode` | Rotation or sound mode, and the visualizers available |
| `PUT` | `/mode` | Switch mode or pick a visualizer |
| `GET` | `/slots` | The rotation and the slot types that can go in it |
| `PUT` | `/slots` | Replace the rotation |
| `GET` | `/transition` | The transition played between slots |
| `PUT` | `/transition` | Choose the transition |
| `POST` | `/wifi/setup` | Restart into the Wi-Fi setup hotspot |
| `POST` | `/update` | Install new firmware sent from a PC |
| `GET` | `/uploads` | Art uploaded to the card |
| `POST` | `/uploads` | Upload a GIF |
| `GET` | `/uploads/{id}.gif` | One upload's GIF |
| `POST` | `/uploads/{id}/show` | Show an upload now |
| `DELETE` | `/uploads/{id}` | Delete an upload |
| `GET` | `/clock` | How the clock looks |
| `PUT` | `/clock` | Change how the clock looks |
| `POST` | `/clock/show` | Show the clock now, to see how it looks |
| `GET` | `/weather` | The weather settings and the latest reading |
| `PUT` | `/weather` | Set the key, town, units or refresh |
| `GET` | `/weather/search` | Towns matching a name, to choose from |
| `GET` | `/starter` | The starter art built into the firmware |
| `GET` | `/starter/{id}.gif` | One piece of starter art |
| `POST` | `/starter/{id}/show` | Show a piece of starter art now |
| `GET` | `/starter/{id}/still.gif` | One frame of a piece, for thumbnails |
| `GET` | `/auth` | Whether a PIN is set and this client is signed in (open) |
| `POST` | `/auth/login` | Sign in with the PIN (open) |
| `POST` | `/auth/logout` | Sign this browser out (open) |
| `PUT` | `/auth/pin` | Set, change or remove the PIN |

### `GET /status`

```json
{
  "device": {
    "name": "pixel-jukebox",
    "model": "pixel-jukebox-64x64",
    "firmware": "1.2.3",
    "ip": "192.168.4.105",
    "ssid": "Home Network",
    "rssi": -64,
    "uptimeSeconds": 14
  },
  "slot": { "name": "makapix", "held": false },
  "artwork": {
    "postId": 647,
    "owner": "Odio",
    "gifUrl": "https://vault.makapix.club/21/32/abc123.gif",
    "pageUrl": "https://makapix.club/post/586cbe4d-3161-4dbf-8824-9dc8c317b7b8",
    "source": "makapix",
    "liked": false,
    "channel": "Promoted"
  },
  "screen": "rotation",
  "mode": "rotation",
  "visualizer": "spectrum",
  "channelOverride": null,
  "brightness": 32,
  "night": { "active": false, "panel": "playing", "overridden": false },
  "panel": { "photo": false, "refreshHz": 100, "rotation": 0, "mirror": false }
}
```

| Field | Meaning |
|---|---|
| `device.firmware` | The firmware version, `FIRMWARE_VERSION` in `include/config.h`. The value above is only an example |
| `device.ssid` | The Wi-Fi network the board is on |
| `device.rssi` | Wi-Fi signal strength in dBm |
| `slot.name` | What is on screen: a slot from `GET /slots`, or in sound mode a visualizer from `GET /mode`; `null` between slots or when nothing can be shown |
| `slot.held` | `true` while the current slot is held |
| `artwork` | The Makapix post on screen; `null` when the slot is not showing artwork (clock, registration code) |
| `artwork.owner` | The artist's Makapix handle |
| `artwork.gifUrl` | HTTPS address of the GIF being played, for clients that want to show it. Empty when the artwork is playing from the card |
| `artwork.pageUrl` | The artwork's page on makapix.club (by its storage key; the site redirects to its short `/p/` address). `null` for art saved to the card before 0.35.0, until it is downloaded again |
| `artwork.source` | `makapix` when downloaded for this turn, `card` when played from the board's offline copy, because Makapix or the Wi-Fi could not be reached or because it was asked for from the gallery; `upload` and `starter` for art that is not from Makapix (see Uploads and Starter art) |
| `artwork.liked` | Whether the artwork has been liked from this panel; see `PUT /likes/{postId}` |
| `artwork.channel` | The channel it came from, as `channelLabel` in `GET /slots`; `null` when from the card |
| `weather` | Only while the weather slot is on screen: `{"place": "Georgetown", "units": "f", "temperature": 59.2, "condition": "Sunny"}`, with `temperature` and `condition` `null` when there is no reading |
| `screen` | What has the panel this turn: `rotation`, `request` (one artwork asked for), `sound`, `nightClock` or `dark`. See [What is on screen](#what-is-on-screen) |
| `mode` | `rotation` or `sound` |
| `channelOverride` | `null`, or `{"channel": "hashtag:cat", "from": "makapix.club"}` while one channel plays in place of the rotation's; see `PUT /channel` |
| `visualizer` | The visualizer sound mode uses, whether or not it is showing now |
| `brightness` | 0-255, as showing now: the night schedule may have lowered it |
| `night` | Whether it is night by the schedule, what the panel is doing (`playing`, `clock` or `off`) and whether someone has taken over for a while; see `GET /night` |
| `panel` | Whether photo mode is on, and how often the panel redraws; see `GET /panel` |

### `GET /diagnostics`

Memory and task-stack headroom, for checking limits against measurements. All
sizes are in bytes.

```json
{
  "uptimeSeconds": 318,
  "lastReset": "power on",
  "heap": { "free": 135868, "lowestFree": 70660, "largestBlock": 81908 },
  "psram": { "present": true, "size": 16774227, "free": 16583611, "lowestFree": 16552927, "largestBlock": 16515060 },
  "tasks": [
    { "name": "display", "stack": 8192, "lowestFree": 5864 },
    { "name": "api", "stack": 8192, "lowestFree": 5816 }
  ],
  "artwork": { "downloaded": 42, "largestBytes": 187850 },
  "crash": { "present": true, "task": "loader", "cause": "StoreProhibited" },
  "case": { "present": true, "temperatureC": 31.8, "humidityPercent": 33.0, "lowestC": 24.6,
            "highestC": 32.1, "secondsAgo": 12 },
  "clock": { "source": "network", "utc": "2026-10-16T15:55:14Z",
             "chip": { "present": true, "keptTimeAtStart": true, "lastSetUtc": "2026-10-16T15:54:59Z",
                       "driftSecondsAtLastSet": -1 } },
  "card": { "present": true, "mounted": true, "ready": true, "type": "SD", "capacityBytes": 2032664576,
            "fileSystem": "FAT32", "totalBytes": 2027028480, "freeBytes": 2026909696 },
  "cache": { "enabled": true, "artworks": 11, "bytes": 66985, "budgetBytes": 1073741824,
             "maxArtworks": 1500, "storedSinceStart": 11, "playedSinceStart": 0 }
}
```

| Field | Meaning |
|---|---|
| `lastReset` | Why the board last started: `power on`, `software restart`, `crash`, `watchdog`, `brownout` or `other` |
| `boot` | How long start-up took to reach each step, in ms since power-on: `[{"step": "cache", "ms": 3072}, ...]`, ending with `first slot on screen`. On a full 1,500-artwork card the art appears about 12 s after power-on |
| `heap`, `psram` | Internal and external memory. `lowestFree` is the least that has been free since startup; `largestBlock` is the biggest single allocation possible now |
| `tasks` | `lowestFree` is the least stack each task has had spare. `loader` is short-lived and appears once it has run |
| `artwork` | Artworks downloaded since startup and the size of the largest |
| `case` | Temperature and humidity inside the case, from the board's SHTC3 sensor, read once a minute; `lowestC` and `highestC` since start-up. The board and the panel warm the air around the sensor, so this is how hot the electronics run, not the room: expect several degrees above it, and humidity correspondingly lower. `null` until the first reading; `present` is `false` if the sensor did not answer |
| `firmware` | `version`; `slot`, the flash slot it runs from (`app0` or `app1`); `newAndOnProbation`, true for a just-installed firmware that is not yet kept (see `POST /update`); `updating`, true while an upload is being written |
| `live` | The live connection to makapix.club, as `GET /live` |
| `web` | How the web server is keeping up: `slowestMs` (the slowest request since start-up), `overOneSecond` (how many took longer), `slowestHandlerUri`, and for card pictures `slowestCardReadMs` and `slowestPictureSendMs`, to tell a slow card from slow Wi-Fi |
| `cardLock` | The card lock (shared by card access and TLS): the longest wait for it and the longest hold, with the task names |
| `crash` | Whether a crash record is stored, and if so which task crashed and why. Details from `GET /crash` |
| `clock.source` | Where the time came from: `network` (internet time, checked about hourly), `clock chip` (the board's PCF85063, read at start-up, until the network answers) or `not set`. `utc` is `null` until the time is known |
| `clock.chip` | The clock chip. `keptTimeAtStart` is `false` after the power was off, unless a backup battery is fitted. It is set from the network each time that answers; `driftSecondsAtLastSet` is how far it had wandered by then (a second either way is rounding: it counts whole seconds) |
| `card` | The microSD card. `present` means a card answered; `mounted` that it has a FAT file system the board can read; `ready` that it also passed a write test. When it cannot be used, `problem` says why. The card is never formatted by the board |
| `cache` | Artworks saved on the card. `budgetBytes` is the most the cache will use (at most 1 GB, always leaving 64 MB of the card free); beyond that, or past `maxArtworks`, the artwork that has gone longest unplayed is removed. `playedSinceStart` counts artworks played from the card, offline or asked for from the gallery |

### `GET /crash`

When the board crashes, or a watchdog fires because something stopped
responding, ESP-IDF writes a record to the flash before restarting. It stays
there, through any number of restarts, until erased or replaced by the next
crash, so a crash that happened overnight can still be looked at.

```json
{
  "present": true,
  "task": "loader",
  "cause": "StoreProhibited",
  "causeCode": 29,
  "hint": "wrote to an address that is not memory, often a null or freed pointer",
  "pc": "0x40378182",
  "address": "0x00000000",
  "backtrace": ["0x40378182", "0x4037d529", "0x42020060"],
  "backtraceCorrupted": false,
  "elfSha256": "d6a5f9bb1360638b",
  "matchesRunningFirmware": false,
  "foundUtc": "2026-10-16T17:18:20Z",
  "firmware": "0.15.0",
  "resetReason": "crash",
  "dumpBytes": 20004
}
```

With no record the reply is `{"present": false}`.

| Field | Meaning |
|---|---|
| `task` | The task that crashed: `display`, `api`, `loader`, `reporter` and so on |
| `cause` | The CPU's exception, as the ESP-IDF panic handler names it. `hint` explains the common ones |
| `pc`, `backtrace` | Where it happened. Only meaningful with the build that crashed: `python tools/decode_crash.py` turns them into function names and source lines |
| `address` | The memory address the failing instruction used; `0x00000000` usually means a null pointer |
| `backtraceCorrupted` | The stack was damaged, so the backtrace may stop early |
| `elfSha256` | Identifies the build that crashed. `matchesRunningFirmware` is `false` once a different firmware has been installed |
| `foundUtc`, `firmware`, `resetReason` | When the board first found the record after restarting, what it was running then, and why it had restarted (`crash` or `watchdog`; something else means the record predates that start-up) |

### `GET /crash/dump`

The raw record as stored in flash (`application/octet-stream`), with every
task's stack, for decoding in full with ESP-IDF's `espcoredump.py`; see
`tools/decode_crash.py`. `404` when there is no record.

### `DELETE /crash`

Erases the record. Reply: `200 {"erased":true}`, or `{"erased":false}` when
there was none.

### `GET /likes`

The artworks liked from this panel, the most recent first, with what is still
to be sent to Makapix.

```json
{ "count": 2, "waiting": 0, "sentSinceStart": 3, "refusedSinceStart": 0, "liked": [2486, 647] }
```

| Field | Meaning |
|---|---|
| `count` | Artworks liked |
| `waiting` | Likes and unlikes not yet sent: offline, or Makapix unreachable |
| `refusedSinceStart` | Changes Makapix turned down for good: the post was deleted or hidden, or already has as many reactions as it takes. They are dropped, not retried |
| `problem` | Present while sending is failing, for example `could not reach Makapix` |

Makapix does not tell a player what its owner has already liked, so a like
made on the website shows here only once it has also been liked from the panel.

### `PUT /likes/{postId}`

Likes an artwork on Makapix as the panel's owner, with the ❤️ reaction. No
body. Reply: `200 {"postId":2486,"liked":true}`. The like is remembered on the
board at once and sent in the background, within seconds when online and once
the connection returns otherwise. `409` until the panel is linked to an account.
`GET /status` shows `artwork.liked` for the artwork on screen.

### `DELETE /likes/{postId}`

Takes the like back. Reply: `200 {"postId":2486,"liked":false}`. A like taken
back before it was sent is never sent at all.

### `GET /views`

```json
{ "report": true, "waiting": 0, "sentSinceStart": 41, "refusedSinceStart": 0,
  "rateLimitedSinceStart": 0, "droppedSinceStart": 0 }
```

Each artwork that stays on the panel for 2 seconds is reported to Makapix as a
view, which counts towards the artist's view numbers and the site's trending
lists. Art asked for from the gallery is reported as chosen (`intent:
"artwork"`), everything else as part of the shuffle (`intent: "channel"`).
Makapix takes at most one report every 5 seconds, so they are sent at that pace.
Views played without a connection wait on the card (in memory without one, up
to 1,000) and are sent when it returns. They carry the time they were shown,
from the clock chip when the network has not answered yet; with no time at all
(power lost, no battery, no network), Makapix records them when they arrive.

| Field | Meaning |
|---|---|
| `report` | Whether views are reported. On by default |
| `waiting` | Views not yet sent |
| `refusedSinceStart` | Views Makapix turned down for good (a deleted post, say) |
| `rateLimitedSinceStart` | Views sent too soon after the last and retried, after as long as Makapix asked (its `Retry-After`) |
| `droppedSinceStart` | Views lost because 1,000 were already waiting |
| `problem` | Present while sending is failing |

### `PUT /views`

`{"report": false}` stops reporting views and drops any still waiting;
`{"report": true}` starts again. Saved on the board. Reply: the same document
as `GET /views`. `400` unless `report` is `true` or `false`.

### `GET /night`

```json
{
  "enabled": true,
  "start": "22:00",
  "end": "07:00",
  "action": "dim",
  "brightness": 8,
  "fadeMinutes": 10,
  "now": { "phase": "night", "panel": "playing", "brightness": 8, "nextSwitch": "07:00" }
}
```

Between `start` and `end` (local time; past midnight when `end` is earlier)
the panel does one of three things, fading from the day brightness over
`fadeMinutes` and back again when night ends:

| `action` | At night |
|---|---|
| `dim` | The art keeps playing at `brightness` |
| `clock` | Only the clock, at `brightness`; no transitions |
| `off` | Fades to black, then nothing plays, so no art is downloaded and no views are reported |

Moving the brightness by night (`PUT /brightness`) takes over for an hour or
until the next switch, whichever is sooner, without changing the saved day
level. On a `clock` or `off` night, `POST /next` (or showing art from the
gallery) does the same at the day brightness. The schedule needs the time: with
none known (power lost, no battery, no network) it waits.

| `now` field | Meaning |
|---|---|
| `phase` | `disabled`, `no time`, `day`, `night` or `overridden` |
| `panel` | `playing`, `clock` or `off` |
| `brightness` | The level showing now |
| `nextSwitch` | When night next starts or ends, local time |
| `overrideMinutesLeft` | While `overridden`, how long until the schedule takes over again |

Off by default.

### `PUT /night`

Any of `enabled`, `start`, `end`, `action`, `brightness` and `fadeMinutes`;
the others stay as they are. Saved on the board. Reply: `200` with the
`GET /night` document. `400`, and nothing changes, for a time that is not
`HH:MM`, `start` equal to `end`, an unknown `action`, `brightness` outside
0-255, or `fadeMinutes` outside 0-60. A change ends any override.

### `GET /panel`

```json
{ "photo": false, "refreshHz": 100, "rotation": 0, "mirror": false }
```

`rotation` (0, 90, 180 or 270, clockwise) and `mirror` (left and right
swapped, done before turning) say which way up the picture goes, for however
the panel sits in its case, or for a live stream or a mirror. Everything is
drawn upright and turned on its way to the panel.

The panel draws each color as a sequence of on and off times, many times a
second. Eyes see a steady picture, but a camera's short exposure catches part
of a cycle and shows dark bands rolling through the picture.

| `photo` | Refresh | Use |
|---|---|---|
| `false` | about 100 Hz | The most accurate colors |
| `true` | about 500 Hz | Filming and photographing: cameras see a steady picture. The five dimmest steps of each color get equal time, so dark shades are less smooth |

`refreshHz` is the rate the display driver worked out at start-up.

### `PUT /panel`

Any of `photo`, `rotation` and `mirror`, for example `{"rotation": 90}` or
`{"mirror": true}`. The orientation changes at once and is saved; reply `200`
with the `GET /panel` document.

`{"photo": true}` or `{"photo": false}`: the driver lays out its memory for one
refresh rate when it starts, so the setting is saved and the board restarts to
use it (about 5 s, the API included). Reply: `202 {"photo":true,"rotation":0,"mirror":false,"restarting":true}`.
Asking for the setting already in use changes nothing: `200` with the `GET /panel`
document. `400` with none of the three, or unless `photo` and `mirror` are
`true` or `false` and `rotation` is 0, 90, 180 or 270.

### `GET /card`

The `card` and `cache` blocks of `GET /diagnostics` on their own: whether a
card is present and usable, its size and free space, and the artworks stored
on it for playing offline. The fields are described under `GET /diagnostics`.

### `GET /card/art`

The artworks saved on the card, the most recently played first. Query
parameters: `offset` (default 0) and `limit` (default 24, at most 60).

```json
{
  "artworks": [
    { "postId": 2486, "owner": "Odio", "bytes": 1593, "dwellMs": 30000,
      "pageUrl": "https://makapix.club/post/586cbe4d-3161-4dbf-8824-9dc8c317b7b8" }
  ],
  "offset": 0,
  "total": 100
}
```

| Field | Meaning |
|---|---|
| `total` | Artworks on the card altogether |
| `artworks[].bytes` | Size of the GIF on the card |
| `artworks[].dwellMs` | How long the artist asked for it to be shown |
| `artworks[].pageUrl` | Its page on makapix.club, or `null` when saved before 0.35.0 |

With no usable card the list is empty and `total` is 0.

### `GET /card/art/{postId}.gif`

The GIF as saved on the card, `image/gif`. Browsers may keep it for a day,
since a saved artwork never changes. `404` when it is not on the card.

### `POST /card/art/{postId}/show`

Shows a saved artwork on the panel as soon as the current slot gives way, with
the usual transition, for the artwork's own time. The rotation then carries on
where it was. Releases a hold. No body. Reply: `202 {"queued":"show"}`.
`404` when it is not on the card. In sound mode the artwork has its turn and
then the visualizer comes back. `POST /show/{postId}` does the same for any
Makapix post, on the card or not.

### `DELETE /card/art/{postId}`

Deletes one artwork from the card. Reply: `200 {"removed":2486}`; `404` when
it is not on the card. If it is on the panel it finishes its turn.

### `DELETE /card/art`

Deletes every saved artwork. Reply: `200 {"removed":100}`. Art is saved again
as it plays online.

### `POST /reset`

`{"confirm": "reset"}`. Puts every setting back to its default, as on a new
panel, then restarts: the rotation, the clock and time zone, the weather
(its key too), the night schedule, the display options and brightness, the
transition, view reports and the live switch, the PIN, the Makapix link (the
panel registers again and shows a new code; remove the old player on
makapix.club) and the likes and views still waiting to be sent. Kept: the
Wi-Fi details, everything on the card, and the crash record. Reply:
`202 {"reset":true,"restarting":true}`. `400` without the confirmation, so a
stray request cannot wipe the panel. On the control page: Settings >
Advanced > Reset all settings, with two taps.

### `POST /restart`

Restarts the board. No body. Reply: `202 {"queued":"restart"}`; the API stops
answering for a few seconds while it starts again. The card slot has no
card-detect switch, so a card is only looked for at start-up: restart after
inserting or swapping one.

### `POST /next`

Ends the current slot and moves to the next one in the rotation. On an artwork
slot this shows the next artwork. It also releases a hold. In sound mode it
steps to the next visualizer. No body.

Reply: `202 {"queued":"next"}`

### `POST /hold`

A held slot stays on screen past its normal time; animated artwork keeps
looping. Two small bars show in the top-right corner of the panel.

| Body | Effect | Reply |
|---|---|---|
| `{"held": true}` | Hold | `202 {"queued":"hold"}` |
| `{"held": false}` | Release | `202 {"queued":"release"}` |
| none | Toggle | `202 {"queued":"toggleHold"}` |

Setting the state it already has does nothing. Hold has no effect in sound
mode, where the visualizer already stays on screen. `held` must be a JSON boolean;
anything else is a `400`.

### `GET /brightness`

```json
{ "value": 32 }
```

### `PUT /brightness`

```json
{ "value": 48 }
```

`value` must be a whole number from 0 to 255; anything else is a `400` and the
brightness is left unchanged. Reply: `202 {"queued":"brightness"}`.

The value is saved on the board and restored after a restart. While the night schedule is changing the brightness, it is not saved: it takes over for an hour or until the schedule's next switch (see `GET /night`). `GET /brightness` gives the level showing now.

Brightness is limited by the 5V supply, not by the API. When the supply cannot
keep up, white turns yellow and then red at higher settings. If that happens,
check the panel's power lead is properly connected and the supply is rated
high enough.

### `GET /mode`

The panel is in one of two modes. In `rotation` it works through the slots from
`GET /slots`. In `sound` it shows one microphone-driven visualizer until told
otherwise.

```json
{
  "mode": "rotation",
  "visualizer": "spectrum",
  "visualizers": [
    { "name": "spectrum", "label": "Spectrum bars" },
    { "name": "vu", "label": "VU meter" },
    { "name": "scope", "label": "Oscilloscope" },
    { "name": "fire", "label": "Fire" },
    { "name": "rings", "label": "Pulse rings" },
    { "name": "starfield", "label": "Starfield" },
    { "name": "stereo", "label": "Stereo scope" },
    { "name": "hifi", "label": "Hi-fi meter" }
  ]
}
```

The panel always starts in `rotation` after a restart. The chosen `visualizer`
is saved and restored.

### `PUT /mode`

```json
{ "mode": "sound", "visualizer": "fire" }
```

```json
{ "mode": "rotation" }
```

`mode` is required. `visualizer` is optional and only used with `sound`; leave
it out to keep the current one. Applied directly, not queued: the reply is `200`
with the same body as `GET /mode`, and the panel switches within a moment,
interrupting whatever is on screen. Returning to `rotation` carries on from
where the rotation left off.

It is a `400`, and nothing changes, if `mode` is not one of the two values or
`visualizer` is not a name from the `visualizers` list, even with `rotation`.

### `GET /slots`

```json
{
  "available": [
    { "name": "makapix", "label": "Makapix artwork", "turn": "one artwork" },
    { "name": "clock", "label": "Clock", "turn": "a set time", "seconds": { "min": 5, "max": 60, "default": 10 } },
    { "name": "weather", "label": "Weather", "turn": "a set time", "seconds": { "min": 5, "max": 60, "default": 10 } }
  ],
  "rotation": [
    { "slot": "makapix", "turns": 2, "channel": "promoted", "channelLabel": "Promoted" },
    { "slot": "makapix", "turns": 1, "channel": "likedby:billy", "channelLabel": "Liked by @billy" },
    { "slot": "makapix", "turns": 1, "channel": "artist:nobody", "channelLabel": "@nobody",
      "problem": "User 'nobody' not found" },
    { "slot": "clock", "turns": 1, "enabled": false, "seconds": 10 }
  ],
  "channels": [
    { "type": "all", "label": "Everything" },
    { "type": "promoted", "label": "Promoted" },
    { "type": "mine", "label": "My art" },
    { "type": "likedby", "label": "Liked by", "needs": "name" },
    { "type": "artist", "label": "An artist", "needs": "name" },
    { "type": "hashtag", "label": "A hashtag", "needs": "hashtag" }
  ],
  "maxEntries": 12,
  "maxTurns": 20
}
```

| Field | Meaning |
|---|---|
| `available` | Slot types this firmware has. `turn` says what one turn of that slot is, for showing to people. Timed slots (the clock and the weather) have `seconds`: the range a row's turn may last |
| `rotation[].seconds` | For timed slots: how long one turn lasts, 5 to 60 seconds; 10 when not given. Other slots have none (an artwork's turn is as long as its artist asked) |
| `rotation` | The order slots are shown in. Each entry is shown `turns` times in a row, then the next entry, looping |
| `rotation[].enabled` | `false` while a slot is paused: it keeps its place, turns and channel but is skipped, and nothing is downloaded for it. At least one slot is always on |
| `rotation[].channel` | For `makapix` slots: which Makapix art it plays; see Channels below. `channelLabel` is the same for people |
| `rotation[].problem` | Present while the channel cannot play, in Makapix's words or the board's. Its turns are skipped meanwhile, and Makapix is asked again after 5 minutes |
| `channels` | The channel types, and what each needs after the colon |
| `maxEntries`, `maxTurns` | Limits that `PUT /slots` enforces |

The example plays two featured artworks, one the owner has liked, one by an
artist (skipped: there is no such user), then the clock for 10 seconds, and
repeats; the clock is paused. The same slot type may appear more than once,
with different channels.

#### Channels

| Channel | Plays |
|---|---|
| `all` | Everything public on Makapix. The default |
| `promoted` | Art featured on Makapix |
| `mine` | Art by the account the panel is linked to |
| `artist:<name>` | One artist's art, by their Makapix name |
| `hashtag:<tag>` | Posts carrying a hashtag |
| `likedby:<name>` | Art someone has reacted to, newest reactions included; with your own name, your likes from the website and the panel |
| `uploads` | Art uploaded to the card (see Uploads) |
| `starter` | The starter art built into the firmware (see Starter art) |

Every channel is limited, as before, to art that fits the panel and has a GIF,
shuffled. A name or tag may be written with its `@` or `#`; it is stored
without, and hashtags in lower case. Each channel keeps its own place in its
feed and an artwork ready, so taking turns between channels loses nothing. A
channel with little art loops through it. Without a connection, every artwork
slot plays from the card, whatever its channel. When a slot has nothing to
play at all (offline with nothing on the card, a channel with a `problem`, a
download that has not arrived), it plays starter art rather than being
skipped. View reports say which channel an artwork came from.

### `PUT /slots`

```json
{
  "rotation": [
    { "slot": "makapix", "turns": 3, "channel": "promoted" },
    { "slot": "makapix", "turns": 1, "channel": "hashtag:cat" },
    { "slot": "clock", "turns": 2 }
  ]
}
```

A `makapix` entry without `channel` plays `all`. An entry without `enabled` is on;
`"enabled": false` pauses it.

Replaces the whole rotation. Unlike the commands above it is applied directly,
not queued: the reply is `200` with the same body as `GET /slots`. The slot on
screen finishes its turn, then the new rotation starts from its first entry.
The rotation is saved on the board and restored after a restart.

It is a `400`, and nothing changes, if `rotation` is not an array of 1 to
`maxEntries` entries, a `slot` is not one of the `available` names, `turns`
is not a whole number from 1 to `maxTurns`, a `channel` is not one of the
forms above, a slot other than `makapix` has one, `enabled` is not `true` or
`false`, or every entry is paused. Whether a name exists is
for Makapix to say: an unknown one is accepted and then shows a `problem`.

### `GET /transition`

When one slot gives way to the next in the rotation, a short transition
(about 0.7 seconds) blends the old picture into the new one.

```json
{
  "transition": "random",
  "available": [
    { "name": "crossfade", "label": "Crossfade" },
    { "name": "mosaic", "label": "Mosaic" }
  ]
}
```

`transition` is `random` (a different effect each time, never the same twice
running; the default), `none`, or one `name` from `available`. The list above
is shortened: there are thirteen, `crossfade`, `fade`, `slide`, `wipe`, `iris`,
`blinds`, `mosaic`, `dissolve`, `melt`, `checkerboard`, `scanline`, `spiral`
and `rain`.

A transition is extra time between slots. A slot's own time on screen is
counted from when it is fully showing, so a 30-second artwork is still shown
for 30 seconds. `POST /next` during a transition cuts it short. Sound mode
does not use transitions.

### `PUT /transition`

```json
{ "transition": "mosaic" }
```

Applied directly: the reply is `200` with the same body as `GET /transition`,
and the choice is used from the next change of slot. It is saved on the board.
It is a `400`, and nothing changes, if `transition` is not `random`, `none` or
a name from `available`.

### `POST /wifi/setup`

Restarts the board into its setup hotspot so a different Wi-Fi network can be
chosen. No body. Reply: `202 {"queued":"wifiSetup"}`.

The board leaves the network, so the API stops answering. It starts an open
network named `Pixel-Jukebox` and serves a setup page there (see the README).
The saved network is kept: if nobody connects to the hotspot for three minutes,
the board restarts and rejoins it. Holding the BOOT button for five seconds does
the same thing.

## Uploads

GIFs uploaded from a phone or PC are kept on the card in `jukebox/uploads/`,
apart from the art cache, so they are never evicted and play with or without
Wi-Fi. The `uploads` channel plays them, shuffled, in an artwork slot or as
`PUT /channel`. They are not Makapix posts, so no views or likes are sent.
Up to 200, each a GIF (GIF87a or GIF89a) of 64 x 64 or smaller and at most
512 KB; 16 x 16 and 32 x 32 art is scaled up like Makapix art. The Art tab of
the control page uploads, shows and removes them.

While an upload is on screen, `GET /status` has `artwork` as
`{"postId": null, "source": "upload", "upload": {"id": 3, "name": "cat"}, "channel": "My uploads"}`.

### `GET /uploads`

```json
{ "available": true, "count": 2, "max": 200, "bytes": 924,
  "uploads": [ { "id": 2, "name": "sun _ moon", "width": 64, "height": 64, "bytes": 259 } ] }
```

Newest first. `available` is false with no usable card.

### `POST /uploads`

The body is the GIF itself, sent as for `POST /update`
(`Content-Type: application/octet-stream`). Its name goes in `X-File-Name`,
percent-encoded (`encodeURIComponent`); it is tidied to letters, digits,
spaces, dots, dashes and underscores, 40 at most, without `.gif`. The header
is checked before anything is kept. Reply: `201` with the upload, as in the
list. `400` for something that is not a GIF, is bigger than 64 x 64 or 512 KB,
or is empty; `409` with 200 uploads already or one in progress; `507` when the
card is full.

```sh
curl -H "Content-Type: application/octet-stream" -H "X-File-Name: cat.gif" --data-binary @cat.gif http://pixel-jukebox.local/api/v1/uploads
```

### `GET /uploads/{id}.gif`

The GIF as uploaded. `404` for no such upload.

### `POST /uploads/{id}/show`

Shows it for one turn, like `POST /show/{postId}`. Reply: `202 {"queued":"showUpload"}`.

### `DELETE /uploads/{id}`

Reply: `200 {"removed": 3}`. `404` for no such upload.

## Clock

The clock slot shows the time in one of three styles, with the date if
wanted. Its look is one setting for the whole panel, used by every clock row
in the rotation and by the night schedule's clock-only nights; how long each
clock row stays on screen is that row's `seconds`.

### `GET /clock`

```json
{
  "style": "digital",
  "hours": 12,
  "seconds": false,
  "date": "weekday",
  "color": "timeofday",
  "customColor": "#ff8000",
  "timezone": "America/Chicago",
  "localTime": "2026-10-22 18:42",
  "choices": {
    "style": ["digital", "big", "analog"],
    "date": ["off", "weekday", "daymonth", "numeric"],
    "color": ["white", "amber", "mint", "timeofday", "custom"],
    "timezone": [ { "name": "America/Chicago", "label": "US Central (Chicago)" } ]
  }
}
```

| Field | Meaning |
|---|---|
| `style` | `digital`: the time at twice the text size, the date above and AM/PM below. `big`: chunky digits across the panel, readable across a room. `analog`: a dial with hands, and the day of the month in a window at three o'clock |
| `hours` | 12 or 24. A 24-hour clock has no AM/PM. The analog dial ignores it |
| `seconds` | Show the seconds: under the time, or as a second hand |
| `date` | `off`, `weekday` ("Thu Oct 22"), `daymonth` ("22 Oct") or `numeric` ("10/22"). The analog dial shows only the day |
| `localTime` | The panel's local time in that zone, to check it; `null` until the time is known |
| `timezone` | The time zone of the clock and the night schedule, by its IANA name; one of the 48 in `choices.timezone` (the common ones, with daylight saving). US Central until one is chosen. A change takes effect at once |
| `customColor` | The color used when `color` is `custom`, as `"#rrggbb"`. Very dark colors show coarser on the panel, which has fewer steps near black |
| `color` | `white`, `amber`, `mint`, `custom` (`customColor`), or `timeofday`: a dim red at night that does not light the room, orange at dawn, white at midday, amber in the evening, changing smoothly |

### `PUT /clock`

Any of the fields above; the rest stay as they are. A clock on screen shows
the change at once. Reply: the `GET /clock` document. `400` for a value not
listed.

### `POST /clock/show`

Shows the clock for one turn, as a request (like `POST /show/{postId}`).
Reply: `202 {"queued":"showClock"}`.

## Weather

The weather slot shows the current conditions where you are: a pixel-art
picture of the weather and the temperature. They come from
[WeatherAPI.com](https://www.weatherapi.com/) over HTTPS, with a free key of your
own (one per app, under their terms), every 15 to 60 minutes (30 by default),
and only while a `weather` slot is in the rotation, switched on, and the panel
is not off for the night. WeatherAPI.com's terms allow a reading to be kept for
60 minutes: after that the slot shows "no data", and before that, once a check
has been missed, the reading's age. Credit: "Weather by WeatherAPI.com" (their
terms require it on the free plan).

The key is entered in Settings or with `PUT /weather`, or built in from
`WEATHERAPI_KEY` in `include/secrets.h`; Settings wins. It is never sent back.

### `GET /weather`

```json
{
  "keySet": true,
  "keySource": "settings",
  "place": { "name": "Georgetown", "region": "Texas", "country": "United States of America", "lat": 30.63, "lon": -97.68 },
  "units": "f",
  "refreshMinutes": 30,
  "refreshMin": 15,
  "refreshMax": 60,
  "reading": { "tempC": 15.1, "tempF": 59.2, "condition": "Sunny", "code": 1000, "icon": "clear", "isDay": true, "ageSeconds": 42, "timezone": "America/Chicago" },
  "problem": null,
  "inRotation": true,
  "credit": "Weather by WeatherAPI.com"
}
```

| Field | Meaning |
|---|---|
| `keySource` | `settings`, `build` (from `secrets.h`) or `none` |
| `place` | The town, as chosen from `GET /weather/search`; `null` until one is |
| `reading` | The latest conditions, `null` when there are none under 60 minutes old. `code` is WeatherAPI.com's condition code; `icon` the picture the panel shows for it: `clear`, `partly-cloudy`, `cloudy`, `haze`, `fog`, `drizzle`, `rain`, `snow`, `sleet` or `storm` (clear and partly cloudy have night versions) |
| `reading.timezone` | The town's time zone, as WeatherAPI.com gives it. Settings offers it for the clock when they differ |
| `problem` | Why there is no reading: `no WeatherAPI.com key`, `no town chosen`, `the key was refused`, `this month's free calls are used up`..., or `null` |
| `inRotation` | Whether a weather slot is in the rotation, switched on: without one nothing is fetched |

### `PUT /weather`

Any of `key`, `place`, `units` (`"f"` or `"c"`) and `refreshMinutes` (15 to
60); the rest stay as they are. `place` is an entry from `GET /weather/search`.
A new key or place is checked at once; units need no new check, since every
reading has both. `"key": ""` removes the key set here, falling back to the
built-in one. Reply: the `GET /weather` document. `400` for a key that is not
letters and digits, a place without a name, `lat` and `lon`, or anything else
out of range.

### `GET /weather/search`

`?q=Georgetown`: towns matching the name, up to 8, asked of WeatherAPI.com
(a second or two). Choose from these rather than sending a name: an unknown
name is not an error at WeatherAPI.com but quietly becomes some other place.

```json
{ "places": [ { "name": "Georgetown", "region": "Texas", "country": "United States of America", "lat": 30.63, "lon": -97.68 } ] }
```

`400` for fewer than 2 or more than 60 characters, or with no key; `502` when
WeatherAPI.com refuses or cannot be reached, with its reason.

## Starter art

Art built into the firmware, so a new panel has something to show before it
is linked to Makapix, and any panel has something to show offline with an
empty card: 28 pieces of 64 x 64 animated GIFs, from CC0 pixel art packs and
some drawn by code (`tools/make_starter_art.py`; credits in `CREDITS.md`).
The `starter` channel plays them, shuffled. An artwork slot with nothing else
to play plays them too, and before the panel is linked its turns go between
the registration code and starter art. Not Makapix posts: no views or likes.
The Built-in view of the control page's Art tab shows them all.

While a piece is on screen, `GET /status` has `artwork` as
`{"postId": null, "source": "starter", "starter": {"id": 7, "title": "Pineapple, doubled", "credit": "Pixel Frog, Pixel Adventure 1"}, "channel": "Starter art"}`.
`channel` is `null` when it is shown for want of Makapix art.

### `GET /starter`

```json
{ "count": 28, "bytes": 1146070,
  "pieces": [ { "id": 1, "title": "Ninja Frog runs", "credit": "Pixel Frog, Pixel Adventure 1" } ] }
```

### `GET /starter/{id}.gif`

The GIF. `404` for no such piece.

### `GET /starter/{id}/still.gif`

One frame of the piece as a small GIF (about 1.4 KB, against 40 KB for a
whole piece), for thumbnails. `404` for no such piece.

### `POST /starter/{id}/show`

Shows it for one turn, like `POST /show/{postId}`; the Built-in view of the
Art tab does this. Reply: `202 {"queued":"showStarter"}`. `404` for no such piece.

### `POST /update`

Installs new firmware sent from a PC, so no USB cable is needed. The body is
the raw `firmware.bin` from `pio run`
(`.pio/build/waveshare-s3-rgb-matrix/firmware.bin`), sent with
`Content-Type: application/octet-stream` and a `Content-Length`. An optional
`X-Firmware-MD5` header (32 hex digits) is checked against what arrived.
`tools/ota_upload.py` does all of this, and the Settings tab of the control
page has an Update button.

```sh
curl -H "Content-Type: application/octet-stream" --data-binary @.pio/build/waveshare-s3-rgb-matrix/firmware.bin http://pixel-jukebox.local/api/v1/update
```

The flash has two firmware slots. The upload is written into the one not
running, while the panel shows "Update receiving" and a percentage, and is then
checked as a whole. Only then is it chosen for the next start-up: the reply is
`200 {"updated":true,"restarting":true}` and the board restarts into it.

The new firmware starts on probation. It is kept once its API has been up for
30 seconds. If it crashes, hangs, restarts or does not get its API up within
10 minutes, the board goes back to the previous firmware. Until it is kept, a
second update is refused (`409`), so there is always a firmware to go back to.
`GET /diagnostics` shows the slot and whether it is still on probation.

Like every other route it needs the PIN when one is set; the PIN is checked
before anything is written. `400` for an empty body, a body sent as a form,
or a file that is not firmware for this board (nothing is changed), `409`
while an update is in progress or on probation. USB uploads keep working and are
the way back if the panel ever stops answering.

## What is on screen

Several things can take over the panel from the rotation. One set of rules
decides between them (`lib/jukebox_core/screen_rules.h`), and `GET /status`
names the winner as `screen`:

| `screen` | When | Next | Hold |
|---|---|---|---|
| `request` | An artwork was asked for: the gallery, `POST /back`, `POST /show/{postId}`, or makapix.club. Comes first, and wakes a dark panel. One turn, then whatever was on carries on | Ends it | Yes |
| `dark` | The night schedule has the panel off | Wakes it | No |
| `sound` | Sound mode is on | Next visualizer | No |
| `nightClock` | A clock-only night | Ends the turn | Yes |
| `rotation` | Otherwise | Next slot | Yes |

A channel set with `PUT /channel` (or from makapix.club) is not a separate
screen: the rotation keeps its order and its clock, and every artwork slot
plays that channel until it is cleared, the rotation is changed, or the board
restarts.

### `POST /back`

Shows the artwork before the current one, for one turn; pressing it again goes
further back (up to 16). No body. Reply: `202 {"queued":"back"}`. Nothing
happens when there is nothing to go back to.

### `POST /show/{postId}`

Shows any Makapix post for one turn: from the card when it is there, otherwise
fetched from Makapix (and then saved to the card). The last picture stays up
while it downloads. No body. Reply: `202 {"queued":"show"}`. A post that does
not exist, is hidden, or is bigger than the panel is skipped, and the panel
carries on; the serial log says why.

### `GET /channel`

```json
{ "channel": "hashtag:cat", "label": "#cat", "from": "api" }
```

All three are `null` when every artwork slot plays its own channel. `from` is
`makapix.club` when the website chose it, `api` otherwise.

### `PUT /channel`

`{"channel": "hashtag:cat"}`, in the form used by `PUT /slots`. Every artwork
slot plays it from its next turn, in place of its own channel. Not saved: a
restart, `DELETE /channel` or `PUT /slots` goes back to the rotation's
channels. Reply: as `GET /channel`. `400` for something that is not a channel.

### `DELETE /channel`

Back to the rotation's own channels. Reply: as `GET /channel`.

## Test patterns

For judging the picture: comparing normal and photo mode, checking white
balance, and spotting brightness steps out of order (`docs/color.md`). Every
color is exact in RGB565, the form artwork reaches the display driver in.

| Pattern | Shows |
|---|---|
| `ramps` | Grey, red, green and blue, dark to light, in four bands |
| `darks` | All 32 grey levels artwork can have, 16 bands to a half: each band should be brighter than the one before |
| `bars` | White, yellow, cyan, green, magenta, red, blue, black |
| `white` | All white, for white balance |

### `GET /test`

`{"showing": false, "patterns": [{"name": "ramps", "label": "Ramps"}, ...]}`

### `PUT /test`

`{"pattern": "darks"}`. Shown at once (no transition), even at night, until
Next, `DELETE /test` or ten minutes. Reply: `202 {"queued":"testPattern"}`.
`400` for an unknown pattern.

### `DELETE /test`

Back to whatever was on before. Reply: `202 {"queued":"reload"}`, or
`200 {"showing": false}` when no pattern is up.

## Live on makapix.club

The panel keeps a connection to Makapix's MQTT broker (`makapix.club:8883`,
mutual TLS with the panel's own certificate). With it, the website shows the
panel as online with the artwork it is showing (sent on every change and every
minute, and `offline` when the connection drops), and the website can control
it:

| From makapix.club | The panel |
|---|---|
| Next (`swap_next`) | As `POST /next` |
| Back (`swap_back`) | As `POST /back` |
| Send to player (`show_artwork`) | As `POST /show/{postId}` |
| Play a channel (`play_channel`): all, promoted, an artist, a hashtag, liked by someone | Leaves sound mode and plays it as `PUT /channel`, at once |
| Play a playset (`play_playset`) | Not supported yet: received and ignored |

While the night schedule has the panel off, it goes offline on makapix.club
too: it says goodbye and disconnects, and connects again in the morning, or as
soon as the panel is woken from the control page or its button. So the website
cannot wake a panel that is off for the night. A clock-only or dimmed night
stays connected.

Art, likes and view reports still go over HTTPS. The certificate is fetched
with the panel's token the first time, kept on the board, and renewed by the
panel when it has 30 days left or when the broker turns it away three times in
a row (at most once a day). After a drop the panel reconnects after 1 s, then
2, 4 ... up to 5 minutes.

### `GET /live`

```json
{ "enabled": true, "state": "connected", "connectedSeconds": 140, "reconnects": 0,
  "commandsReceived": 2, "lastCommand": { "type": "swap_next", "secondsAgo": 12, "actedOn": true },
  "certificateExpires": "2029-10-13" }
```

| Field | Meaning |
|---|---|
| `state` | `connected`, `connecting`, `retrying`, `night` (offline while the night schedule has the panel off), `waiting` (for Wi-Fi or for the panel to be linked) or `off` |
| `reconnects` | Connections made since start-up after the first |
| `lastCommand` | The last command from the website; `actedOn` is false for one the panel does not support |
| `certificateExpires` | When the panel's MQTT certificate runs out (UTC date) |
| `problem` | Present while it cannot connect, saying why |

### `PUT /live`

`{"enabled": false}` tells the broker the panel is going offline and
disconnects; it stays off across restarts. `{"enabled": true}` connects again.
On by default. Reply: as `GET /live`. `400` unless `enabled` is `true` or
`false`.

## The PIN

There is no PIN until one is set, and the API is open to anyone on the network,
as it always was. With one set (4 to 8 digits), every route except `GET /api/v1`
and the `/auth` routes answers `401` unless the request carries one of:

- the `jukebox_session` cookie that `POST /auth/login` sets (what the control
  page uses),
- `Authorization: Bearer <session>` with that cookie's value, or
- the PIN itself in an `X-Jukebox-Pin` header, for scripts. It counts as a
  guess, like a sign-in.

Five wrong PINs in a row refuse every PIN for 30 seconds: `429` with a
`Retry-After` header. Browsers already signed in carry on. The PIN is stored as
a salted SHA-256, never as itself. Up to eight browsers stay signed in, through
restarts; a ninth signs out the one seen longest ago. The control page itself
(`GET /`) is open and shows a PIN prompt.

The setup hotspot is not covered (it is a separate, short-lived network). That
is also the way back from a forgotten PIN: hold the BOOT button for five
seconds, join `Pixel-Jukebox` and tick "Also remove the control-page PIN" on the
setup page. The tick box is only offered when setup was asked for (the button,
or `POST /wifi/setup` by a signed-in client), not when the board fell back to
setup because it could not join its network.

### `GET /auth`

```json
{ "pinSet": true, "signedIn": false, "lockedSeconds": 0 }
```

`signedIn` is true when the request's session is valid, or always when no PIN
is set. An `X-Jukebox-Pin` header is ignored here, so asking never counts as a
guess.

### `POST /auth/login`

`{"pin": "1234"}`. Reply: the same document as `GET /auth`, with a
`Set-Cookie: jukebox_session=...` header (a year, `HttpOnly`,
`SameSite=Strict`). `401` for a wrong PIN, `429` while locked. With no PIN set
it answers `200` and sets nothing.

### `POST /auth/logout`

Ends this browser's session and clears the cookie. Reply: the same document as
`GET /auth`.

### `PUT /auth/pin`

`{"pin": "5678", "current": "1234"}` sets or changes the PIN; `{"pin": "",
"current": "1234"}` removes it. `current` is only needed when a PIN is set, and
the request must itself be signed in. Every browser is signed out, the one
asking too (the control page signs straight back in with the new PIN). Reply:
the same document as `GET /auth`. `400` for a PIN that is not 4 to 8 digits,
`403` when `current` is wrong, `429` while locked.

## Error responses

| Status | When |
|---|---|
| `400` | Body is not a JSON object, or a field has the wrong type or range |
| `401` | A PIN is set and the request has no valid session or PIN |
| `403` | `PUT /auth/pin` with the wrong current PIN |
| `404` | Unknown path, or a known path with the wrong method |
| `429` | Five wrong PINs in a row: wait for `Retry-After` seconds |
| `503` | The command queue is full |

## Examples

```sh
curl http://pixel-jukebox.local/api/v1/status
curl -X POST http://pixel-jukebox.local/api/v1/next
curl -X POST -H "Content-Type: application/json" -d '{"held":true}' http://pixel-jukebox.local/api/v1/hold
curl -X PUT -H "Content-Type: application/json" -d '{"value":32}' http://pixel-jukebox.local/api/v1/brightness
curl http://pixel-jukebox.local/api/v1/slots
curl -H "X-Jukebox-Pin: 1234" http://pixel-jukebox.local/api/v1/status   # with a PIN set
```
