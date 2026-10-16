"""Exercise the Pixel Jukebox API against a real board.

    python tools/api_smoke_test.py                  # uses pixel-jukebox.local
    python tools/api_smoke_test.py 192.168.4.105

Checks every endpoint in docs/jukebox-api.md except POST /wifi/setup, which
would take the board off the network. Settings it changes (brightness, mode,
rotation, transition) are put back afterwards. Standard library only. Exits non-zero if
any check fails.
"""

import json
import sys
import time
import urllib.error
import urllib.request

HOST = sys.argv[1] if len(sys.argv) > 1 else "pixel-jukebox.local"
BASE = f"http://{HOST}/api/v1"
SETTLE_S = 1.0  # commands are queued; give the display loop time to act

failures = []
checks = 0


def call(method, path, body=None, raw=None, base=BASE):
    data = raw if raw is not None else (json.dumps(body).encode() if body is not None else None)
    headers = {"Content-Type": "application/json"} if data is not None else {}
    request = urllib.request.Request(base + path, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=8) as response:
            status, payload = response.status, response.read()
    except urllib.error.HTTPError as error:
        status, payload = error.code, error.read()
    try:
        return status, json.loads(payload)
    except json.JSONDecodeError:
        return status, payload.decode("utf-8", "replace")


def check(name, condition, detail=""):
    global checks
    checks += 1
    if condition:
        print(f"  ok    {name}")
    else:
        print(f"  FAIL  {name}  {detail}")
        failures.append(name)


def status():
    return call("GET", "/status")[1]


def wait_for(predicate, seconds=10):
    deadline = time.time() + seconds
    while time.time() < deadline:
        if predicate(status()):
            return True
        time.sleep(0.4)
    return False


def section(title):
    print(f"\n{title}")


print(f"Testing {BASE}")
try:
    code, first = call("GET", "/status")
except OSError as error:
    sys.exit(f"Cannot reach the board at {HOST}: {error}")

original_brightness = first["brightness"]
original_rotation = call("GET", "/slots")[1]["rotation"]
original_mode = call("GET", "/mode")[1]
original_transition = call("GET", "/transition")[1]["transition"]

try:
    section("Discovery and pages")
    code, body = call("GET", "")
    check("GET /api/v1 names the API", code == 200 and body.get("api") == "/api/v1", body)
    code, body = call("GET", "/", base=f"http://{HOST}")
    check("GET / serves the control page", code == 200 and "<title>Pixel Jukebox</title>" in body)
    code, body = call("GET", "/nope")
    check("unknown path is 404 with an error", code == 404 and "error" in body, body)
    code, body = call("GET", "/next")
    check("wrong method is 404", code == 404, body)

    section("Status and diagnostics")
    check("status has device, slot, mode and brightness",
          all(key in first for key in ("device", "slot", "artwork", "mode", "visualizer", "brightness")),
          list(first))
    check("device block is complete",
          all(key in first["device"] for key in ("name", "model", "firmware", "ip", "ssid", "rssi", "uptimeSeconds")))
    code, body = call("GET", "/diagnostics")
    check("diagnostics reports heap, PSRAM and tasks",
          code == 200 and body["psram"]["present"] and body["heap"]["free"] > 0 and len(body["tasks"]) >= 3, body)
    check("diagnostics shows no crash since startup",
          body.get("lastReset") not in ("crash", "watchdog", "brownout"), body.get("lastReset"))

    code, body = call("GET", "/card")
    check("card status reports the card and the cache",
          code == 200 and "present" in body.get("card", {}) and "enabled" in body.get("cache", {}), body)

    code, body = call("GET", "/card/art?limit=2")
    check("saved art lists at most the page asked for",
          code == 200 and isinstance(body.get("artworks"), list) and len(body["artworks"]) <= 2
          and isinstance(body.get("total"), int), body)
    if body.get("artworks"):
        first = body["artworks"][0]["postId"]
        image = urllib.request.urlopen(f"{BASE}/card/art/{first}.gif", timeout=10).read()
        check("a saved artwork's image is a GIF", image[:4] == b"GIF8", image[:16])
    code, body = call("POST", "/card/art/999999999/show")
    check("showing an artwork that is not on the card is refused", code == 404, body)
    code, body = call("DELETE", "/card/art/abc")
    check("removing with a postId that is not a number is refused", code == 404, body)

    section("Makapix")
    code, body = call("GET", "/likes")
    check("likes list the liked artworks",
          code == 200 and isinstance(body.get("liked"), list) and isinstance(body.get("waiting"), int), body)
    code, body = call("PUT", "/likes/abc")
    check("liking with a postId that is not a number is refused", code == 404, body)
    code, views = call("GET", "/views")
    check("view reports show whether they are on", code == 200 and isinstance(views.get("report"), bool), views)
    code, body = call("PUT", "/views", {"report": "yes"})
    check("view reports reject a value that is not true or false", code == 400, body)
    code, body = call("PUT", "/views", {"report": views.get("report", True)})
    check("view reports accept their current value", code == 200 and body.get("report") == views.get("report"), body)
    code, now = call("GET", "/status")
    if now.get("artwork"):
        check("the artwork on screen says whether it is liked", isinstance(now["artwork"].get("liked"), bool),
              now["artwork"])

    section("Mode")
    call("PUT", "/mode", {"mode": "rotation"})
    check("starts from rotation mode", wait_for(lambda s: s["mode"] == "rotation" and s["slot"]["name"], 25))
    code, body = call("GET", "/mode")
    names = [v["name"] for v in body.get("visualizers", [])]
    check("mode lists the visualizers", code == 200 and names == ["spectrum", "vu", "scope", "fire"], names)
    for name in names:
        code, body = call("PUT", "/mode", {"mode": "sound", "visualizer": name})
        shown = wait_for(lambda s: s["mode"] == "sound" and s["slot"]["name"] == name)
        check(f"sound mode shows {name}", code == 200 and shown)
    call("POST", "/next")
    check("next steps to the following visualizer", wait_for(lambda s: s["slot"]["name"] == "spectrum"))
    call("POST", "/hold", {"held": True})
    time.sleep(SETTLE_S)
    check("hold is ignored in sound mode", status()["slot"]["held"] is False)
    for bad in ({"mode": "party"}, {}, {"mode": "sound", "visualizer": "nope"}, {"mode": "sound", "visualizer": 5}):
        code, body = call("PUT", "/mode", bad)
        check(f"mode rejects {json.dumps(bad)}", code == 400 and "error" in body, body)
    code, body = call("PUT", "/mode", {"mode": "rotation"})
    check("returns to rotation mode", code == 200 and wait_for(lambda s: s["mode"] == "rotation" and s["slot"]["name"], 25))

    section("Hold and next")
    for held in (True, False):
        code, body = call("POST", "/hold", {"held": held})
        check(f"hold {held} is queued and applied",
              code == 202 and wait_for(lambda s, held=held: s["slot"]["held"] is held), body)
    code, body = call("POST", "/hold")
    check("hold with no body toggles", code == 202 and wait_for(lambda s: s["slot"]["held"] is True), body)
    code, body = call("POST", "/next")
    check("next is queued and releases the hold", code == 202 and wait_for(lambda s: s["slot"]["held"] is False), body)
    for bad in ({"held": "yes"}, {"held": 1}):
        code, body = call("POST", "/hold", bad)
        check(f"hold rejects {json.dumps(bad)}", code == 400, body)
    code, body = call("POST", "/hold", raw=b"{not json")
    check("hold rejects malformed JSON", code == 400, body)

    section("Brightness")
    target = 20 if original_brightness != 20 else 21
    code, body = call("PUT", "/brightness", {"value": target})
    check("brightness is queued and applied",
          code == 202 and wait_for(lambda s: s["brightness"] == target), body)
    check("GET /brightness agrees", call("GET", "/brightness")[1].get("value") == target)
    for bad in ({"value": 256}, {"value": -1}, {"value": 3.5}, {"value": "40"}, {}):
        code, body = call("PUT", "/brightness", bad)
        check(f"brightness rejects {json.dumps(bad)}", code == 400, body)
    check("rejected values changed nothing", call("GET", "/brightness")[1].get("value") == target)

    section("Rotation")
    code, body = call("GET", "/slots")
    available = [slot["name"] for slot in body.get("available", [])]
    check("slots lists what can go in the rotation", code == 200 and available == ["makapix", "clock"], available)
    trial = [{"slot": "clock", "turns": 2}, {"slot": "makapix", "turns": 1}]
    code, body = call("PUT", "/slots", {"rotation": trial})
    check("rotation can be replaced", code == 200 and body.get("rotation") == trial, body)
    check("GET /slots returns the new rotation", call("GET", "/slots")[1]["rotation"] == trial)
    too_many = [{"slot": "clock", "turns": 1}] * (body.get("maxEntries", 8) + 1)
    for label, bad in (
        ("an empty list", {"rotation": []}),
        ("an unknown slot", {"rotation": [{"slot": "nope", "turns": 1}]}),
        ("a visualizer", {"rotation": [{"slot": "fire", "turns": 1}]}),
        ("zero turns", {"rotation": [{"slot": "clock", "turns": 0}]}),
        ("too many turns", {"rotation": [{"slot": "clock", "turns": 21}]}),
        ("a missing field", {"rotation": [{"slot": "clock"}]}),
        ("a non-list", {"rotation": "clock"}),
        ("too many entries", {"rotation": too_many}),
    ):
        code, body = call("PUT", "/slots", bad)
        check(f"rotation rejects {label}", code == 400 and "error" in body, body)
    check("rejected rotations changed nothing", call("GET", "/slots")[1]["rotation"] == trial)

    section("Transitions")
    code, body = call("GET", "/transition")
    names = [item["name"] for item in body.get("available", [])]
    check("transition lists 13 effects", code == 200 and len(names) == 13 and len(set(names)) == 13, names)
    for choice in ("none", "random", names[0], names[-1]):
        code, body = call("PUT", "/transition", {"transition": choice})
        check(f"transition can be set to {choice}",
              code == 200 and call("GET", "/transition")[1]["transition"] == choice, body)
    for bad in ({"transition": "nope"}, {"transition": 3}, {}):
        code, body = call("PUT", "/transition", bad)
        check(f"transition rejects {json.dumps(bad)}", code == 400 and "error" in body, body)
    check("rejected values changed nothing", call("GET", "/transition")[1]["transition"] == names[-1])

finally:
    section("Restoring settings")
    call("PUT", "/slots", {"rotation": original_rotation})
    call("PUT", "/brightness", {"value": original_brightness})
    call("PUT", "/mode", {"mode": original_mode["mode"], "visualizer": original_mode["visualizer"]}
         if original_mode["mode"] == "sound" else {"mode": "rotation"})
    call("POST", "/hold", {"held": False})
    call("PUT", "/transition", {"transition": original_transition})
    time.sleep(SETTLE_S)
    restored = (call("GET", "/slots")[1]["rotation"] == original_rotation
                and call("GET", "/brightness")[1]["value"] == original_brightness
                and call("GET", "/mode")[1]["mode"] == original_mode["mode"]
                and call("GET", "/transition")[1]["transition"] == original_transition)
    check("rotation, brightness, mode and transition are back as they were", restored)

print(f"\n{checks - len(failures)} of {checks} checks passed")
if failures:
    print("Failed: " + "; ".join(failures))
    sys.exit(1)
