"""Show the board's last crash, with function names and source lines.

    python tools/decode_crash.py                       # pixel-jukebox.local
    python tools/decode_crash.py 192.168.4.129
    python tools/decode_crash.py --dump crash.bin      # also save the full record

Reads GET /api/v1/crash and runs the program counter and backtrace through
addr2line against the build that crashed. Every build keeps a copy of its ELF
in .pio/elf-archive/ (tools/keep_elf.py), named by its SHA-256, which is what
the crash record names; the current build is the fallback, with a warning
when it is not the one that crashed. Builds made before the archive existed
cannot be decoded.

The full record (--dump) can be read with ESP-IDF's espcoredump.py, which
shows every task's stack and variables:

    espcoredump.py info_corefile -t raw -c crash.bin .pio/elf-archive/<elfSha256>.elf

Erase the record afterwards with: curl -X DELETE http://pixel-jukebox.local/api/v1/crash
"""

import argparse
import hashlib
import json
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ELF = ROOT / ".pio/build/waveshare-s3-rgb-matrix/firmware.elf"
ARCHIVE = ROOT / ".pio/elf-archive"
ADDR2LINE = Path.home() / ".platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-addr2line"


def get(url):
    with urllib.request.urlopen(url, timeout=15) as response:
        return response.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("host", nargs="?", default="pixel-jukebox.local")
    parser.add_argument("--dump", metavar="FILE", help="also save the raw crash record to FILE")
    args = parser.parse_args()
    base = f"http://{args.host}/api/v1"

    crash = json.loads(get(base + "/crash"))
    if not crash.get("present"):
        print("No crash record on the board.")
        return 0

    print(f"Crash in task '{crash.get('task', '?')}': {crash.get('cause', '?')} (EXCCAUSE {crash.get('causeCode', '?')})")
    if crash.get("hint"):
        print(f"  {crash['hint']}")
    print(f"  address {crash.get('address')}, found {crash.get('foundUtc') or 'at an unknown time'}, "
          f"running {crash.get('firmware') or '?'}, reset reason: {crash.get('resetReason') or '?'}")

    if args.dump:
        Path(args.dump).write_bytes(get(base + "/crash/dump"))
        print(f"  full record saved to {args.dump} ({crash.get('dumpBytes')} bytes)")

    addresses = [crash["pc"]] + [a for a in crash.get("backtrace", []) if a != crash["pc"]]
    recorded = crash.get("elfSha256", "")
    elf = ARCHIVE / f"{recorded[:16]}.elf" if recorded else None
    if elf and elf.exists():
        print(f"  decoded with the build that crashed ({elf.relative_to(ROOT)})")
    elif ELF.exists():
        elf = ELF
        local = hashlib.sha256(ELF.read_bytes()).hexdigest()
        if recorded and not local.startswith(recorded):
            print(f"\nWarning: the build that crashed ({recorded}) is not in {ARCHIVE.relative_to(ROOT)}, and the")
            print(f"current build ({local[:len(recorded)]}) is a different one. Function names below may be wrong.")
    else:
        print("\nNo build to decode the addresses with; build the firmware first. Addresses:")
        print("  " + " ".join(addresses))
        return 0

    tool = ADDR2LINE.with_suffix(".exe") if sys.platform == "win32" else ADDR2LINE
    lines = subprocess.run([str(tool), "-pfiaC", "-e", str(elf)] + addresses,
                           capture_output=True, text=True).stdout.strip().splitlines()
    print("\nBacktrace" + (" (may be cut short: the stack was damaged)" if crash.get("backtraceCorrupted") else "") + ":")
    for line in lines:
        print("  " + line.replace(str(ROOT) + "\\", "").replace(str(ROOT) + "/", ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
