"""PlatformIO post-build step: keep a copy of every firmware ELF.

A crash record names the build that crashed only by the SHA-256 of its ELF,
and its addresses can only be turned into function names with that exact
file. Builds are overwritten all the time, so each one is copied to
.pio/elf-archive/<first 16 hex digits of its SHA-256>.elf, where
tools/decode_crash.py looks for it. Each is about 23 MB (full debug
information), so only the newest 15 are kept, about 350 MB.
"""

import hashlib
import shutil
from pathlib import Path

Import("env")  # noqa: F821 (provided by PlatformIO)

KEEP = 15


def keep(target, source, env):  # noqa: ARG001 (PlatformIO's callback signature)
    elf = Path(str(target[0]))
    archive = Path(env.subst("$PROJECT_DIR")) / ".pio" / "elf-archive"
    archive.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(elf.read_bytes()).hexdigest()[:16]
    copy = archive / f"{digest}.elf"
    if not copy.exists():
        shutil.copyfile(elf, copy)
    copy.touch()  # newest first when pruning
    old = sorted(archive.glob("*.elf"), key=lambda p: p.stat().st_mtime, reverse=True)[KEEP:]
    for path in old:
        path.unlink()


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", keep)  # noqa: F821
