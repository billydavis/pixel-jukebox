"""Send new firmware to the panel over Wi-Fi, then check it came back.

    python tools/ota_upload.py                       # pixel-jukebox.local, the last build
    python tools/ota_upload.py 192.168.4.129
    python tools/ota_upload.py 192.168.4.129 --file some/other/firmware.bin

Build first with `pio run`. Uploads .pio/build/waveshare-s3-rgb-matrix/firmware.bin
to POST /api/v1/update with its MD5, waits for the panel to restart, checks the
version it reports against include/config.h, and waits until the new firmware
is kept (it is on probation for its first 30 seconds with the API up, and goes
back to the previous firmware if it does not get that far).

With a PIN set on the panel, put it in the JUKEBOX_PIN environment variable.
USB still works as before (pio run -t upload), and is the way back if the panel
ever stops answering. Standard library only.
"""

import argparse
import hashlib
import http.client
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_FILE = ROOT / ".pio/build/waveshare-s3-rgb-matrix/firmware.bin"
CHUNK = 16 * 1024
COME_BACK_S = 120
KEPT_S = 90


def config_version():
    text = (ROOT / "include/config.h").read_text(encoding="utf-8")
    match = re.search(r'#define FIRMWARE_VERSION "([^"]+)"', text)
    return match.group(1) if match else None


def get(host, path, pin):
    request = urllib.request.Request(f"http://{host}/api/v1{path}", headers={"X-Jukebox-Pin": pin} if pin else {})
    with urllib.request.urlopen(request, timeout=5) as response:
        return json.loads(response.read())


def upload(host, data, pin):
    headers = {
        "Content-Type": "application/octet-stream",
        "Content-Length": str(len(data)),
        "X-Firmware-MD5": hashlib.md5(data).hexdigest(),
    }
    if pin:
        headers["X-Jukebox-Pin"] = pin
    connection = http.client.HTTPConnection(host, 80, timeout=60)
    connection.putrequest("POST", "/api/v1/update")
    for key, value in headers.items():
        connection.putheader(key, value)
    connection.endheaders()
    sent = 0
    for start in range(0, len(data), CHUNK):
        connection.send(data[start:start + CHUNK])
        sent += len(data[start:start + CHUNK])
        print(f"\r  sent {sent * 100 // len(data):3d}%", end="", flush=True)
    print()
    response = connection.getresponse()
    body = response.read().decode("utf-8", "replace")
    connection.close()
    return response.status, body


def main():
    parser = argparse.ArgumentParser(description="Send new firmware to the panel over Wi-Fi.")
    parser.add_argument("host", nargs="?", default="pixel-jukebox.local")
    parser.add_argument("--file", type=Path, default=DEFAULT_FILE)
    args = parser.parse_args()
    pin = os.environ.get("JUKEBOX_PIN", "")

    if not args.file.exists():
        sys.exit(f"{args.file} not found. Build first: pio run")
    data = args.file.read_bytes()
    built = time.strftime("%Y-%m-%d %H:%M", time.localtime(args.file.stat().st_mtime))
    expected = config_version() if args.file == DEFAULT_FILE else None

    try:
        before = get(args.host, "/diagnostics", pin)["firmware"]
    except urllib.error.HTTPError as error:
        sys.exit(f"The panel answered {error.code}" + (": set JUKEBOX_PIN" if error.code == 401 else ""))
    except OSError as error:
        sys.exit(f"Cannot reach the panel at {args.host}: {error}")
    print(f"Panel at {args.host} runs {before['version']} from {before['slot']}; sending {args.file.name} ({len(data) // 1024} KB, built {built})")

    status, body = upload(args.host, data, pin)
    if status != 200:
        sys.exit(f"Refused ({status}): {body}")
    print("  written and checked; the panel is restarting")

    deadline = time.time() + COME_BACK_S
    now = None
    time.sleep(5)
    while time.time() < deadline:
        try:
            now = get(args.host, "/diagnostics", pin)["firmware"]
            break
        except (OSError, KeyError, ValueError):
            time.sleep(3)
    if now is None:
        sys.exit(f"The panel did not come back within {COME_BACK_S} s. If it stays away, upload over USB.")
    print(f"  back, running {now['version']} from {now['slot']}")
    if now["slot"] == before["slot"]:
        sys.exit("It is still running from the same slot: the new firmware did not start, so the old one is back.")
    if expected and now["version"] != expected:
        sys.exit(f"Expected {expected}: the panel went back to its previous firmware.")

    deadline = time.time() + KEPT_S
    while now.get("newAndOnProbation") and time.time() < deadline:
        time.sleep(5)
        try:
            now = get(args.host, "/diagnostics", pin)["firmware"]
        except (OSError, KeyError, ValueError):
            pass
    if now.get("newAndOnProbation"):
        sys.exit("Still on probation after 90 s; check /api/v1/diagnostics.")
    if now["slot"] == before["slot"]:
        sys.exit(f"The panel restarted into {now['slot']}: it went back to its previous firmware.")
    print("  kept: the update is done")


if __name__ == "__main__":
    main()
