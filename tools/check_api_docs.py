"""Check that every API route in the firmware is documented and in the Postman collection.

    python tools/check_api_docs.py

Reads the routes registered in src/network/api.cpp and looks for each one in
docs/jukebox-api.md and postman/Pixel-Jukebox-API.postman_collection.json, and
for anything those two describe that the firmware does not have. Needs no
board. Exits non-zero if they disagree.
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = "/api/v1"


def firmware_routes():
    source = (ROOT / "src/network/api.cpp").read_text(encoding="utf-8")
    routes = set()
    for path, method in re.findall(r'server\.on\(API_BASE(?:\s*"([^"]*)")?,\s*HTTP_(\w+)', source):
        routes.add((method, path))
    return routes


def documented_routes():
    text = (ROOT / "docs/jukebox-api.md").read_text(encoding="utf-8")
    table = text[text.index("## Endpoints"):]
    table = table[:table.index("###")]
    routes = set()
    for method, path in re.findall(r"^\| `(\w+)` \| (.+?) \|", table, re.M):
        match = re.fullmatch(r"`(/[^`]*)`", path)
        routes.add((method, match.group(1) if match else ""))  # the base path row has no path of its own
    return routes


def postman_routes():
    collection = json.loads((ROOT / "postman/Pixel-Jukebox-API.postman_collection.json").read_text(encoding="utf-8"))
    routes = set()
    for folder in collection["item"]:
        if folder["name"] == "Error cases":
            continue  # deliberately includes paths that do not exist
        for item in folder["item"]:
            request = item["request"]
            path = "/" + "/".join(request["url"]["path"])
            if path.startswith(BASE):
                routes.add((request["method"], path[len(BASE):]))
    return routes


def show(routes):
    return ", ".join(sorted(f"{method} {BASE}{path}" for method, path in routes))


firmware = firmware_routes()
problems = []
for name, described in (("docs/jukebox-api.md", documented_routes()), ("the Postman collection", postman_routes())):
    missing = firmware - described
    extra = described - firmware
    if missing:
        problems.append(f"Missing from {name}: {show(missing)}")
    if extra:
        problems.append(f"In {name} but not in the firmware: {show(extra)}")

print(f"{len(firmware)} routes in src/network/api.cpp")
if problems:
    print("\n".join(problems))
    sys.exit(1)
print("The API document and the Postman collection both cover all of them.")
