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
    ("Rotation", [
        ("Get Slots", "GET", "slots", "The rotation and the slot types available.", None, 200),
        ("Set Rotation (default)", "PUT", "slots",
         "Three artworks, then the clock for 10 seconds. Saved on the board; starts after the current slot finishes.",
         {"rotation": DEFAULT_ROTATION}, 200),
        ("Set Rotation (art only)", "PUT", "slots", "Artwork only, no clock.",
         {"rotation": [{"slot": "makapix", "turns": 1}]}, 200),
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
    ("Error cases", [
        ("Brightness Out Of Range", "PUT", "brightness", "Expects 400; the brightness is left unchanged.",
         {"value": 300}, 400),
        ("Hold With Wrong Type", "POST", "hold", "Expects 400; held must be a JSON boolean.", {"held": "yes"}, 400),
        ("Unknown Slot", "PUT", "slots", "Expects 400; the rotation is left unchanged.",
         {"rotation": [{"slot": "nope", "turns": 1}]}, 400),
        ("Unknown Endpoint", "GET", "nope", "Expects 404.", None, 404),
        ("Artwork Not On The Card", "POST", "card/art/999999999/show", "Expects 404; nothing is shown.", None, 404),
        ("View Reports With Wrong Type", "PUT", "views", "Expects 400; reporting is left as it was.",
         {"report": "yes"}, 400),
        ("Unknown Visualizer", "PUT", "mode", "Expects 400; the mode is left unchanged.",
         {"mode": "sound", "visualizer": "nope"}, 400),
        ("Unknown Transition", "PUT", "transition", "Expects 400; the transition is left unchanged.",
         {"transition": "nope"}, 400),
    ]),
]


def request(name, method, path, description, body, expected):
    segments = API.split("/") + (path.split("/") if path else [])
    item = {
        "method": method,
        "header": [],
        "url": {
            "raw": "http://{{device_ip}}/" + "/".join(segments),
            "protocol": "http",
            "host": ["{{device_ip}}"],
            "path": segments,
        },
        "description": description,
    }
    if body is not None:
        item["header"] = [{"key": "Content-Type", "value": "application/json"}]
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
