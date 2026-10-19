"""PlatformIO pre-build step: pack assets/starter/ into .pio/embed/starter_art.bin.

The firmware embeds that one file (board_build.embed_files) and reads it with
lib/jukebox_core/starter_pack.h, which describes the format. The pieces, each
with its still, go in the order of assets/starter/manifest.json, which
tools/make_starter_art.py writes. Titles and credits are cut to 255 bytes,
whole characters only.
"""

import json
import struct
from pathlib import Path

Import("env")  # noqa: F821 (provided by PlatformIO)

project = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
source = project / "assets" / "starter"
staged = project / ".pio" / "embed"
staged.mkdir(parents=True, exist_ok=True)


def text(value):
    raw = value.encode("utf-8")
    while len(raw) > 255:
        value = value[:-1]
        raw = value.encode("utf-8")
    return bytes([len(raw)]) + raw


manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
if len(manifest) > 255:
    raise SystemExit("starter art: at most 255 pieces")
gifs = []
for piece in manifest:
    for key in ("file", "still"):
        gif = (source / piece[key]).read_bytes()
        if gif[:4] != b"GIF8":
            raise SystemExit("starter art: %s is not a GIF" % piece[key])
        gifs.append(gif)

index_size = 6 + sum(16 + len(text(p["title"])) + len(text(p["credit"])) for p in manifest)
index = bytearray(b"PJSA" + bytes([2, len(manifest)]))
offset = index_size
for i, piece in enumerate(manifest):
    gif, still = gifs[2 * i], gifs[2 * i + 1]
    index += struct.pack("<IIII", offset, len(gif), offset + len(gif), len(still))
    index += text(piece["title"]) + text(piece["credit"])
    offset += len(gif) + len(still)
assert len(index) == index_size

pack = bytes(index) + b"".join(gifs)
target = staged / "starter_art.bin"
if not target.exists() or target.read_bytes() != pack:
    target.write_bytes(pack)
