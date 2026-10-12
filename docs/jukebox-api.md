# Jukebox API specification

The REST control surface of the Pixel Jukebox firmware. Any client on the same
network can use it: a web page, a companion app, a script.

Status: **implemented and verified on real hardware.** Source: `src/api.cpp`.

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

`GET /` serves the built-in control page (`src/web_page.h`), which is itself a
client of this API.

## Conventions

- Requests and responses are JSON (`Content-Type: application/json`).
- Errors are `{"error": "<message>"}` with a 4xx or 5xx status.
- There is no authentication. Anyone on the network can call the API.
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

## Endpoints

| Method | Path | Purpose |
|---|---|---|
| `GET` | (the base path, `/api/v1`) | Device name and API base path |
| `GET` | `/status` | What the panel is showing, plus device information |
| `POST` | `/next` | Skip to the next slot |
| `POST` | `/hold` | Hold or release the current slot |
| `GET` | `/brightness` | Read the brightness |
| `PUT` | `/brightness` | Set the brightness |
| `GET` | `/slots` | The rotation and the slot types that can go in it |
| `PUT` | `/slots` | Replace the rotation |
| `POST` | `/wifi/setup` | Restart into the Wi-Fi setup hotspot |

### `GET /status`

```json
{
  "device": {
    "name": "pixel-jukebox",
    "model": "pixel-jukebox-64x64",
    "firmware": "0.6.0",
    "ip": "192.168.4.105",
    "ssid": "Home Network",
    "rssi": -64,
    "uptimeSeconds": 14
  },
  "slot": { "name": "makapix", "held": false },
  "artwork": {
    "postId": 647,
    "owner": "Odio",
    "gifUrl": "https://vault.makapix.club/21/32/abc123.gif"
  },
  "brightness": 32
}
```

| Field | Meaning |
|---|---|
| `device.ssid` | The Wi-Fi network the board is on |
| `device.rssi` | Wi-Fi signal strength in dBm |
| `slot.name` | `makapix` or `clock`; `null` between slots or when nothing can be shown |
| `slot.held` | `true` while the current slot is held |
| `artwork` | The Makapix post on screen; `null` when the slot is not showing artwork (clock, registration code) |
| `artwork.owner` | The artist's Makapix handle |
| `artwork.gifUrl` | HTTPS address of the GIF being played, for clients that want to show it |
| `brightness` | 0-255 |

### `POST /next`

Ends the current slot and moves to the next one in the rotation. On an artwork
slot this shows the next artwork. It also releases a hold. No body.

Reply: `202 {"queued":"next"}`

### `POST /hold`

A held slot stays on screen past its normal time; animated artwork keeps
looping. Two small bars show in the top-right corner of the panel.

| Body | Effect | Reply |
|---|---|---|
| `{"held": true}` | Hold | `202 {"queued":"hold"}` |
| `{"held": false}` | Release | `202 {"queued":"release"}` |
| none | Toggle | `202 {"queued":"toggleHold"}` |

Setting the state it already has does nothing. `held` must be a JSON boolean;
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

The value is saved on the board and restored after a restart.

Brightness is limited by the 5V supply, not by the API. When the supply cannot
keep up, white turns yellow and then red. On the board's USB-C power port this
starts above about 32.

### `GET /slots`

```json
{
  "available": [
    { "name": "makapix", "label": "Makapix artwork", "turn": "one artwork" },
    { "name": "clock", "label": "Clock", "turn": "10 seconds" }
  ],
  "rotation": [
    { "slot": "makapix", "turns": 3 },
    { "slot": "clock", "turns": 1 }
  ],
  "maxEntries": 8,
  "maxTurns": 20
}
```

| Field | Meaning |
|---|---|
| `available` | Slot types this firmware has. `turn` says what one turn of that slot is, for showing to people |
| `rotation` | The order slots are shown in. Each entry is shown `turns` times in a row, then the next entry, looping |
| `maxEntries`, `maxTurns` | Limits that `PUT /slots` enforces |

The example plays three artworks, then the clock for 10 seconds, and repeats.
The same slot type may appear more than once.

### `PUT /slots`

```json
{
  "rotation": [
    { "slot": "makapix", "turns": 5 },
    { "slot": "clock", "turns": 2 }
  ]
}
```

Replaces the whole rotation. Unlike the commands above it is applied directly,
not queued: the reply is `200` with the same body as `GET /slots`. The slot on
screen finishes its turn, then the new rotation starts from its first entry.
The rotation is saved on the board and restored after a restart.

It is a `400`, and nothing changes, if `rotation` is not an array of 1 to
`maxEntries` entries, a `slot` is not one of the `available` names, or `turns`
is not a whole number from 1 to `maxTurns`.

### `POST /wifi/setup`

Restarts the board into its setup hotspot so a different Wi-Fi network can be
chosen. No body. Reply: `202 {"queued":"wifiSetup"}`.

The board leaves the network, so the API stops answering. It starts an open
network named `Pixel-Jukebox` and serves a setup page there (see the README).
The saved network is kept: if nobody connects to the hotspot for three minutes,
the board restarts and rejoins it. Holding the BOOT button for five seconds does
the same thing.

## Error responses

| Status | When |
|---|---|
| `400` | Body is not a JSON object, or a field has the wrong type or range |
| `404` | Unknown path, or a known path with the wrong method |
| `503` | The command queue is full |

## Examples

```sh
curl http://pixel-jukebox.local/api/v1/status
curl -X POST http://pixel-jukebox.local/api/v1/next
curl -X POST -H "Content-Type: application/json" -d '{"held":true}' http://pixel-jukebox.local/api/v1/hold
curl -X PUT -H "Content-Type: application/json" -d '{"value":32}' http://pixel-jukebox.local/api/v1/brightness
curl http://pixel-jukebox.local/api/v1/slots
```
