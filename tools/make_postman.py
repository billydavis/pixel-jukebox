"""Generate the Postman collection for the Pixel Jukebox API.

    python tools/make_postman.py            # rewrite the collection
    python tools/make_postman.py --check    # fail if the file is out of date

The requests are listed below; edit them here, not in the JSON. Every request
builds its URL from the `device_ip` collection variable and carries a test for
the status code it expects. tools/check_api_docs.py then confirms the
collection covers every route in the firmware.
"""

import json
import sys
from pathlib import Path

OUTPUT = Path(__file__).resolve().parent.parent / "postman/Pixel-Jukebox-API.postman_collection.json"
API = "api/v1"
DEFAULT_ROTATION = [{"slot": "makapix", "turns": 3}, {"slot": "clock", "turns": 1}]

# (folder, [(name, method, path, description, body, expected status)])
FOLDERS = [
    ("Status", [
        ("Get Status", "GET", "status",
         "Current slot, hold state, the artwork on screen, brightness and device information.", None, 200),
        ("Get Diagnostics", "GET", "diagnostics",
         "Memory and task-stack headroom since startup, and why the board last restarted.", None, 200),
        ("Get API Pointer", "GET", "", "Returns the device name and the API base path.", None, 200),
        ("Get Crash Record", "GET", "crash",
         "The last crash or watchdog reset, if one is stored: task, cause, addresses and backtrace. "
         "tools/decode_crash.py turns the addresses into function names.", None, 200),
        ("Download Crash Dump", "GET", "crash/dump",
         "The raw crash record, for espcoredump.py. 404 when there is none.", None, 200),
        ("Clear Crash Record", "DELETE", "crash", "Erases the stored crash record.", None, 200),
    ]),
    ("Control", [
        ("Next", "POST", "next",
         "Skips to the next slot. Also releases a hold. Queued: read Get Status to see the result.", None, 202),
        ("Hold", "POST", "hold", "Holds the current slot on screen.", {"held": True}, 202),
        ("Release", "POST", "hold", "Releases a held slot.", {"held": False}, 202),
        ("Toggle Hold", "POST", "hold", "With no body, switches between held and released.", None, 202),
        ("Back", "POST", "back", "Shows the artwork before the current one, for one turn.", None, 202),
        ("Show Any Post", "POST", "show/{{post_id}}",
         "Shows any Makapix post for one turn, from the card or fetched from Makapix.", None, 202),
        ("Get Channel Override", "GET", "channel",
         "The channel playing in every artwork slot in place of the rotation's own, or nulls.", None, 200),
        ("Play One Channel", "PUT", "channel",
         "Every artwork slot plays this channel for now (not saved).", {"channel": "promoted"}, 200),
        ("Back To The Rotation's Channels", "DELETE", "channel", "Clears the channel override.", None, 200),
    ]),
    ("Mode", [
        ("Get Mode", "GET", "mode",
         "The current mode, the chosen visualizer and the visualizers available.", None, 200),
        ("Sound Mode", "PUT", "mode", "Shows the current visualizer until the mode changes.", {"mode": "sound"}, 200),
        ("Sound Mode: Spectrum Bars", "PUT", "mode", "Switches to sound mode with spectrum bars.",
         {"mode": "sound", "visualizer": "spectrum"}, 200),
        ("Sound Mode: VU Meter", "PUT", "mode", "Switches to sound mode with the VU meter.",
         {"mode": "sound", "visualizer": "vu"}, 200),
        ("Sound Mode: Oscilloscope", "PUT", "mode", "Switches to sound mode with the oscilloscope.",
         {"mode": "sound", "visualizer": "scope"}, 200),
        ("Sound Mode: Fire", "PUT", "mode", "Switches to sound mode with fire.",
         {"mode": "sound", "visualizer": "fire"}, 200),
        ("Back To Rotation", "PUT", "mode", "Returns to the rotation (Makapix artwork and clock).",
         {"mode": "rotation"}, 200),
    ]),
    ("Brightness", [
        ("Get Brightness", "GET", "brightness", "Reads the brightness (0-255).", None, 200),
        ("Set Brightness", "PUT", "brightness",
         "Sets the brightness (0-255). Saved on the board.",
         {"value": 32}, 202),
    ]),
    ("Night", [
        ("Get Night Schedule", "GET", "night",
         "The night schedule's settings, and whether it is night now.", None, 200),
        ("Night: Dim 22:00-07:00", "PUT", "night",
         "Dims the art to 8 from 22:00 to 07:00, fading over 10 minutes.",
         {"enabled": True, "start": "22:00", "end": "07:00", "action": "dim", "brightness": 8, "fadeMinutes": 10},
         200),
        ("Night: Clock Only", "PUT", "night", "Shows only the clock at night. Other settings stay as they are.",
         {"action": "clock"}, 200),
        ("Night: Panel Off", "PUT", "night",
         "Turns the panel off at night; Next wakes it for an hour or until morning.", {"action": "off"}, 200),
        ("Night Schedule Off", "PUT", "night", "Turns the schedule off.", {"enabled": False}, 200),
    ]),
    ("Panel", [
        ("Get Panel Settings", "GET", "panel",
         "Whether photo mode is on, the refresh rate, and which way up the picture goes.", None, 200),
        ("Rotate 180", "PUT", "panel", "Turns the picture upside down, at once. Saved.", {"rotation": 180}, 200),
        ("Mirror On", "PUT", "panel", "Swaps left and right, for a live stream or a mirror. Saved.",
         {"mirror": True}, 200),
        ("Upright", "PUT", "panel", "Back to as drawn: no turn, no mirror.", {"rotation": 0, "mirror": False}, 200),
        ("Photo Mode On", "PUT", "panel",
         "Redraws about five times as often, so cameras film the panel without dark bands. "
         "Saved; the board restarts to switch.", {"photo": True}, 202),
        ("Photo Mode Off", "PUT", "panel",
         "Back to the most accurate colours. Saved; the board restarts to switch.", {"photo": False}, 202),
    ]),
    ("Rotation", [
        ("Get Slots", "GET", "slots", "The rotation and the slot types available.", None, 200),
        ("Set Rotation (default)", "PUT", "slots",
         "Three artworks, then the clock for 10 seconds. Saved on the board; starts after the current slot finishes.",
         {"rotation": DEFAULT_ROTATION}, 200),
        ("Set Rotation (art only)", "PUT", "slots", "Artwork only, no clock.",
         {"rotation": [{"slot": "makapix", "turns": 1}]}, 200),
        ("Set Rotation (one paused)", "PUT", "slots",
         "Three artworks and the clock, with the clock paused: it keeps its place and turns but is skipped. "
         "At least one slot must stay on.",
         {"rotation": [{"slot": "makapix", "turns": 3}, {"slot": "clock", "turns": 1, "enabled": False}]}, 200),
        ("Set Rotation (channels)", "PUT", "slots",
         "Two featured artworks, one tagged #cat, one by @Fab, then the clock. Channels: all, promoted, mine, "
         "artist:<name>, hashtag:<tag>, likedby:<name> (your own name for your likes).",
         {"rotation": [{"slot": "makapix", "turns": 2, "channel": "promoted"},
                       {"slot": "makapix", "turns": 1, "channel": "hashtag:cat"},
                       {"slot": "makapix", "turns": 1, "channel": "artist:Fab"},
                       {"slot": "clock", "turns": 1}]}, 200),
    ]),
    ("Transitions", [
        ("Get Transition", "GET", "transition",
         "The transition used between slots, and the effects available.", None, 200),
        ("Transition: Random", "PUT", "transition",
         "A different effect each time a slot changes. The default.", {"transition": "random"}, 200),
        ("Transition: None", "PUT", "transition", "Slots change instantly.", {"transition": "none"}, 200),
        ("Transition: Mosaic", "PUT", "transition",
         "Always uses one effect. Any name from Get Transition works here.", {"transition": "mosaic"}, 200),
    ]),
    ("Makapix", [
        ("Get Likes", "GET", "likes",
         "The artworks liked from this panel, most recent first, and how many changes are still to send.",
         None, 200),
        ("Like Artwork", "PUT", "likes/{{post_id}}",
         "Puts a heart on the artwork on Makapix, as the panel's owner. Sent in the background; "
         "409 until the panel is linked to an account.", None, 200),
        ("Unlike Artwork", "DELETE", "likes/{{post_id}}", "Takes the heart off again.", None, 200),
        ("Get View Reports", "GET", "views",
         "Whether views are reported to Makapix, and how many have been sent or are waiting.", None, 200),
        ("View Reports On", "PUT", "views", "Reports each artwork shown to Makapix. The default.",
         {"report": True}, 200),
        ("View Reports Off", "PUT", "views", "Stops reporting views, and drops any still waiting.",
         {"report": False}, 200),
    ]),
    ("Card", [
        ("Get Card", "GET", "card",
         "The microSD card and the offline artwork cache on it.", None, 200),
        ("List Saved Art", "GET", "card/art",
         "The artworks saved on the card, the most recently played first. Paged with offset and limit "
         "(up to 60); see the API document.", None, 200),
        ("Get Saved Artwork Image", "GET", "card/art/{{post_id}}.gif",
         "The GIF of one saved artwork. Set the post_id variable to a postId from List Saved Art.", None, 200),
        ("Show Saved Artwork", "POST", "card/art/{{post_id}}/show",
         "Shows one saved artwork on the panel now; the rotation then carries on. 409 in sound mode.", None, 202),
        ("Remove Saved Artwork", "DELETE", "card/art/{{post_id}}",
         "Deletes one artwork from the card.", None, 200),
        ("Clear Saved Art", "DELETE", "card/art",
         "Deletes every artwork saved on the card. Art is saved again as it plays online.", None, 200),
        ("Restart Panel", "POST", "restart",
         "Restarts the board. The card is only looked for at start-up, so do this after inserting one.",
         None, 202),
    ]),
    ("Wi-Fi", [
        ("Start Wi-Fi Setup", "POST", "wifi/setup",
         "Restarts the board into its setup hotspot (network name Pixel-Jukebox). The board goes offline; "
         "it rejoins the saved network after three minutes if nobody connects.", None, 202),
    ]),
    ("Test Patterns", [
        ("List Test Patterns", "GET", "test", "The patterns, and whether one is showing.", None, 200),
        ("Show Dark Steps", "PUT", "test", "The 32 grey levels artwork can have; each band should be brighter "
         "than the one before.", {"pattern": "darks"}, 202),
        ("Back To Art", "DELETE", "test", "Ends a test pattern.", None, 202),
    ]),
    ("Live", [
        ("Get Live Connection", "GET", "live",
         "The MQTT connection to makapix.club: state, last command from the website, certificate expiry.",
         None, 200),
        ("Live Connection On", "PUT", "live", "Connects to makapix.club (the default).", {"enabled": True}, 200),
        ("Live Connection Off", "PUT", "live",
         "Says offline to makapix.club and disconnects; stays off across restarts.", {"enabled": False}, 200),
    ]),
    ("Uploads", [
        ("List Uploads", "GET", "uploads", "GIFs uploaded to the card, newest first.", None, 200),
        ("Upload A GIF", "POST", "uploads",
         "As saved it has no file and expects 400. Attach a GIF of 64 x 64 or less under Body > binary, with "
         "Content-Type application/octet-stream and the file name in an X-File-Name header.", None, 400),
        ("Get An Upload's GIF", "GET", "uploads/{{upload_id}}.gif", "One upload's GIF.", None, 200),
        ("Show An Upload", "POST", "uploads/{{upload_id}}/show", "Shows an upload for one turn.", None, 202),
        ("Remove An Upload", "DELETE", "uploads/{{upload_id}}", "Deletes an upload from the card.", None, 200),
        ("Play My Uploads", "PUT", "channel", "Every artwork slot plays your uploads, for now.",
         {"channel": "uploads"}, 200),
    ]),
    ("Weather", [
        ("Get Weather", "GET", "weather",
         "The weather settings and the latest reading. The key is never sent back, only whether one is set.",
         None, 200),
        ("Search For A Town", "GET", "weather/search?q=Georgetown",
         "Towns matching a name, from WeatherAPI.com, to choose from.", None, 200),
        ("Weather In Fahrenheit", "PUT", "weather", "Shows the temperature in Fahrenheit.", {"units": "f"}, 200),
        ("Check Every 30 Minutes", "PUT", "weather", "How often to check: 15 to 60 minutes.",
         {"refreshMinutes": 30}, 200),
    ]),
    ("Starter art", [
        ("List Starter Art", "GET", "starter", "The starter art built into the firmware, with credits.", None, 200),
        ("Get A Starter Piece's GIF", "GET", "starter/{{starter_id}}.gif", "One piece of starter art.", None, 200),
        ("Get A Starter Piece's Still", "GET", "starter/{{starter_id}}/still.gif",
         "One frame of a piece, for thumbnails.", None, 200),
        ("Show A Starter Piece", "POST", "starter/{{starter_id}}/show", "Shows a piece of starter art for one turn.",
         None, 202),
        ("Play Starter Art", "PUT", "channel", "Every artwork slot plays the starter art, for now.",
         {"channel": "starter"}, 200),
    ]),
    ("Firmware", [
        ("Update Firmware", "POST", "update",
         "Installs new firmware. As saved it has no file and expects 400; to really update, attach "
         "firmware.bin under Body > binary with Content-Type application/octet-stream. "
         "tools/ota_upload.py is easier and checks the result.", None, 400),
    ]),
    ("Error cases", [
        ("Brightness Out Of Range", "PUT", "brightness", "Expects 400; the brightness is left unchanged.",
         {"value": 300}, 400),
        ("Hold With Wrong Type", "POST", "hold", "Expects 400; held must be a JSON boolean.", {"held": "yes"}, 400),
        ("Unknown Slot", "PUT", "slots", "Expects 400; the rotation is left unchanged.",
         {"rotation": [{"slot": "nope", "turns": 1}]}, 400),
        ("Every Slot Paused", "PUT", "slots", "Expects 400; at least one slot must stay on.",
         {"rotation": [{"slot": "makapix", "turns": 1, "enabled": False}]}, 400),
        ("Unknown Channel", "PUT", "slots", "Expects 400; the rotation is left unchanged.",
         {"rotation": [{"slot": "makapix", "turns": 1, "channel": "nope"}]}, 400),
        ("Unknown Endpoint", "GET", "nope", "Expects 404.", None, 404),
        ("Night With A Bad Time", "PUT", "night", "Expects 400; the schedule is left unchanged.",
         {"start": "25:00"}, 400),
        ("Photo Mode With Wrong Type", "PUT", "panel",
         "Expects 400; nothing changes and the board does not restart.", {"photo": "yes"}, 400),
        ("Artwork Not On The Card", "POST", "card/art/999999999/show", "Expects 404; nothing is shown.", None, 404),
        ("View Reports With Wrong Type", "PUT", "views", "Expects 400; reporting is left as it was.",
         {"report": "yes"}, 400),
        ("Unknown Visualizer", "PUT", "mode", "Expects 400; the mode is left unchanged.",
         {"mode": "sound", "visualizer": "nope"}, 400),
        ("Unknown Transition", "PUT", "transition", "Expects 400; the transition is left unchanged.",
         {"transition": "nope"}, 400),
    ]),
    ("PIN", [
        ("Get Sign-In State", "GET", "auth",
         "Whether a PIN is set, whether this client is signed in, and any lockout. Always open.", None, 200),
        ("Sign In", "POST", "auth/login",
         "Signs in with the `pin` variable; Postman keeps the session cookie for the requests after it. "
         "With no PIN set it answers 200 and sets nothing.", {"pin": "{{pin}}"}, 200),
        ("Sign Out", "POST", "auth/logout", "Ends this client's session.", None, 200),
        ("Remove PIN", "PUT", "auth/pin",
         "As written, removes the PIN (it needs the `pin` variable to be the current one). Put 4 to 8 digits "
         "in `pin` of the body to set or change it instead. Signs every browser out.",
         {"pin": "", "current": "{{pin}}"}, 200),
    ]),
]


def request(name, method, path, description, body, expected):
    path, _, query = path.partition("?")
    segments = API.split("/") + (path.split("/") if path else [])
    # Ignored while no PIN is set; with one set, fill in the `pin` variable.
    item = {
        "method": method,
        "header": [{"key": "X-Jukebox-Pin", "value": "{{pin}}"}],
        "url": {
            "raw": "http://{{device_ip}}/" + "/".join(segments) + ("?" + query if query else ""),
            "protocol": "http",
            "host": ["{{device_ip}}"],
            "path": segments,
        },
        "description": description,
    }
    if query:
        item["url"]["query"] = [{"key": k, "value": v} for k, _, v in (pair.partition("=") for pair in query.split("&"))]
    if body is not None:
        item["header"].append({"key": "Content-Type", "value": "application/json"})
        item["body"] = {"mode": "raw", "raw": json.dumps(body, indent=2), "options": {"raw": {"language": "json"}}}
    test = [f"pm.test('status is {expected}', function () {{", f"  pm.response.to.have.status({expected});", "});"]
    return {
        "name": name,
        "event": [{"listen": "test", "script": {"type": "text/javascript", "exec": test}}],
        "request": item,
        "response": [],
    }


collection = {
    "info": {
        "name": "Pixel Jukebox API",
        "description": "REST control surface of the Pixel Jukebox firmware. Set the `device_ip` collection "
                       "variable to the board's IP address or to `pixel-jukebox.local`. Full contract: "
                       "docs/jukebox-api.md.",
        "schema": "https://schema.getpostman.com/json/collection/v2.1.0/collection.json",
    },
    "variable": [{
        "key": "device_ip",
        "value": "192.168.4.105",
        "type": "string",
        "description": "The board's IP address or hostname, no scheme or port "
                       "(e.g. 192.168.4.105 or pixel-jukebox.local).",
    }, {
        "key": "post_id",
        "value": "2486",
        "type": "string",
        "description": "A postId from List Saved Art, for the requests on one saved artwork.",
    }, {
        "key": "upload_id",
        "value": "1",
        "type": "string",
        "description": "An id from List Uploads, for the requests on one upload.",
    }, {
        "key": "starter_id",
        "value": "1",
        "type": "string",
        "description": "An id from List Starter Art, 1 up.",
    }, {
        "key": "pin",
        "value": "",
        "type": "string",
        "description": "The panel's PIN, if one is set. Sent in the X-Jukebox-Pin header of every request.",
    }],
    "item": [{"name": folder, "item": [request(*entry) for entry in entries]} for folder, entries in FOLDERS],
}
text = json.dumps(collection, indent=2) + "\n"
count = sum(len(entries) for _, entries in FOLDERS)

if "--check" in sys.argv:
    current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
    if json.loads(current or "{}") != collection:
        print(f"{OUTPUT.name} is out of date. Run: python tools/make_postman.py")
        sys.exit(1)
    print(f"{OUTPUT.name} matches the generator ({count} requests).")
else:
    OUTPUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"Wrote {OUTPUT.name} ({count} requests).")
