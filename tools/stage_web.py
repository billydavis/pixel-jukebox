"""PlatformIO pre-build step: copy web/ into .pio/embed/ before it is embedded.

PlatformIO's embed_txtfiles writes to every file it embeds (it appends a zero
byte, builds, then moves a backup back), so each build rewrites them. On this
project that twice left the real pages in web/ blank after the PC crashed
during a build. Embedding these copies instead means the build only ever
writes to files that can be thrown away.
"""

import shutil
from pathlib import Path

Import("env")  # noqa: F821 (provided by PlatformIO)

project = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
source = project / "web"
staged = project / ".pio" / "embed"
staged.mkdir(parents=True, exist_ok=True)

for page in source.iterdir():
    if page.is_file():
        copy = staged / page.name
        if not copy.exists() or copy.read_bytes() != page.read_bytes():
            shutil.copyfile(page, copy)
