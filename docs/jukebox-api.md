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
`GET /` returns the same pointer: `{"name":"Pixel Jukebox","api":"/api/v1"}`.

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
artwork loads, queued commands wait until playback starts. If the queue is full
the reply is `503` and nothing is queued.

## Endpoints

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/status` | What the panel is showing, plus device information |
| `POST` | `/next` | Skip to the next slot |
| `POST` | `/hold` | Hold or release the current slot |
| `GET` | `/brightness` | Read the brightness |
| `PUT` | `/brightness` | Set the brightness |

### `GET /status`

```json
{
  "device": {
    "name": "pixel-jukebox",
    "model": "pixel-jukebox-64x64",
    "firmware": "0.3.0",
    "ip": "192.168.4.105",
    "rssi": -64,
    "uptimeSeconds": 14
  },
  "slot": { "name": "makapix", "held": false },
  "artwork": { "postId": 647, "owner": "Odio" },
  "brightness": 32
}
```

| Field | Meaning |
|---|---|
| `device.rssi` | Wi-Fi signal strength in dBm |
| `slot.name` | `makapix` or `clock`; `null` between slots or when nothing can be shown |
| `slot.held` | `true` while the current slot is held |
| `artwork` | The Makapix post on screen; `null` when the slot is not showing artwork (clock, registration code) |
| `artwork.owner` | The artist's Makapix handle |
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
```
