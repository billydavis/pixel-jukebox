"""Check the pages in web/ for mistakes a browser would not report.

    python tools/check_web.py

Looks for duplicate element IDs, script and label references to IDs that do not
exist, anything loaded from outside the board, placeholders the firmware does
not fill in, and colour tokens that differ between the control page and the
setup pages. Needs no board. Exits non-zero if anything is wrong.
"""

import re
import sys
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"
PAGES = ["index.html", "setup.html", "setup_check.html", "setup_done.html"]
STYLESHEET = "setup.css"
TOKENS = ["--bg", "--panel", "--edge", "--ink", "--dim", "--accent", "--accent-ink", "--held"]

problems = []


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = []
        self.label_targets = []
        self.links = []  # (tag, attribute, value)
        self.open_tags = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.append(attrs["id"])
        if tag == "label" and "for" in attrs:
            self.label_targets.append(attrs["for"])
        for name in ("src", "href", "action"):
            if attrs.get(name):
                self.links.append((tag, name, attrs[name]))
        if tag not in ("meta", "link", "input", "img", "br"):
            self.open_tags.append(tag)

    def handle_endtag(self, tag):
        if self.open_tags and self.open_tags[-1] == tag:
            self.open_tags.pop()
        else:
            problems.append(f"{self.name}: </{tag}> does not match the open tag "
                            f"<{self.open_tags[-1] if self.open_tags else 'none'}>")


def is_external(value):
    return bool(re.match(r"^(https?:)?//", value))


def tokens(text):
    block = re.search(r":root\s*\{(.*?)\}", text, re.S)
    found = dict(re.findall(r"(--[\w-]+)\s*:\s*([^;}]+)", block.group(1))) if block else {}
    return {name: found.get(name, "").strip() for name in TOKENS}


sizes = {}
texts = {}
for name in PAGES + [STYLESHEET]:
    texts[name] = (WEB / name).read_text(encoding="utf-8")
    sizes[name] = len(texts[name].encode("utf-8"))

for name in PAGES:
    text = texts[name]
    page = Page()
    page.name = name
    page.feed(text)
    if page.open_tags:
        problems.append(f"{name}: unclosed tags: {', '.join(page.open_tags)}")

    for duplicate in sorted({i for i in page.ids if page.ids.count(i) > 1}):
        problems.append(f"{name}: id \"{duplicate}\" is used more than once")

    for target in page.label_targets:
        if target not in page.ids:
            problems.append(f"{name}: a label points at id \"{target}\", which does not exist")

    # IDs the script looks up, through $('x') or getElementById('x').
    for reference in sorted(set(re.findall(r"(?:\$|getElementById)\('([\w-]+)'\)", text))):
        if reference not in page.ids:
            problems.append(f"{name}: the script uses id \"{reference}\", which does not exist")

    for tag, attribute, value in page.links:
        if is_external(value):
            problems.append(f"{name}: <{tag} {attribute}> loads from outside the board: {value}")
    for value in re.findall(r"url\(\s*['\"]?([^'\")]+)", text) + re.findall(r"@import\s+['\"]([^'\"]+)", text):
        if is_external(value):
            problems.append(f"{name}: the stylesheet loads from outside the board: {value}")

# Every placeholder in a setup page must be one the firmware replaces.
firmware = (ROOT / "src/wifi_setup.cpp").read_text(encoding="utf-8")
for name in PAGES:
    for placeholder in sorted(set(re.findall(r"\{\{\w+\}\}", texts[name]))):
        if f'"{placeholder}"' not in firmware:
            problems.append(f"{name}: {placeholder} is never filled in by src/wifi_setup.cpp")

control, setup = tokens(texts["index.html"]), tokens(texts[STYLESHEET])
for name in TOKENS:
    if not control[name] or not setup[name]:
        problems.append(f"colour token {name} is missing from index.html or {STYLESHEET}")
    elif control[name] != setup[name]:
        problems.append(f"colour token {name} differs: index.html has {control[name]}, "
                        f"{STYLESHEET} has {setup[name]}")

for name, size in sizes.items():
    print(f"  {name:<18} {size:>6} bytes")
if problems:
    print("\n" + "\n".join(problems))
    sys.exit(1)
print(f"\n{len(PAGES)} pages and 1 stylesheet checked, no problems found.")
