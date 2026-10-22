"""Draw the weather slot's pixel-art icons and write them as firmware data.

    python -I tools/make_weather_icons.py [--preview preview.png]

Writes src/display/weather_icons.h: one 32 x 32 picture per kind of weather in
lib/jukebox_core/weather_rules.h (Icon), in the same order, then the night
pictures for clear and partly cloudy. Each pixel is RGB565; 0 is black, which
the panel shows as off. Drawn here by code, so they are this project's own.
"""
import argparse
import os

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
N = 32

SUN = (255, 196, 32)
SUN_CORE = (255, 236, 120)
MOON = (230, 228, 196)
STAR = (200, 210, 255)
CLOUD = (226, 230, 240)
CLOUD_SHADE = (150, 158, 178)
GREY = (120, 126, 146)
DARK = (78, 82, 104)
RAIN = (70, 150, 255)
DRIZZLE = (130, 190, 255)
SNOW = (245, 248, 255)
ICE = (170, 230, 255)
BOLT = (255, 220, 40)
HAZE = (214, 170, 96)
FOG = (170, 176, 190)


def canvas():
    return Image.new("RGB", (N, N), (0, 0, 0))


def disc(d, cx, cy, r, fill):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=fill)


def sun(d, cx, cy, r, rays=True):
    if rays:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            length = 3 if dx and dy else 4
            start = r + 2
            d.line((cx + dx * start, cy + dy * start, cx + dx * (start + length - 1), cy + dy * (start + length - 1)),
                   fill=SUN)
    disc(d, cx, cy, r, SUN)
    disc(d, cx - 1, cy - 1, max(1, r - 3), SUN_CORE)


def moon(d, cx, cy, r):
    disc(d, cx, cy, r, MOON)
    disc(d, cx + r // 2 + 1, cy - r // 2, r - 1, (0, 0, 0))


def star(d, x, y):
    d.point((x, y), fill=STAR)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        d.point((x + dx, y + dy), fill=(110, 120, 170))


def cloud(d, x, y, w, body=CLOUD, shade=CLOUD_SHADE):
    """A cloud `w` wide whose flat bottom is at y."""
    h = w // 2
    d.rounded_rectangle((x, y - h // 2, x + w - 1, y), radius=h // 3, fill=body)
    disc(d, x + w // 3, y - h // 2, w // 4, body)
    disc(d, x + (w * 2) // 3, y - h // 2 - 1, w // 3 - 1, body)
    d.line((x + 2, y, x + w - 3, y), fill=shade)
    d.line((x + w - 2, y - h // 2 + 2, x + w - 2, y - 1), fill=shade)


def drops(d, colour, spots, length=3):
    for x, y in spots:
        d.line((x, y, x - 1, y + length - 1), fill=colour)


def flakes(d, colour, spots):
    for x, y in spots:
        d.point((x, y), fill=colour)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            d.point((x + dx, y + dy), fill=colour)


PRECIP_SPOTS = [(9, 22), (15, 24), (21, 22), (12, 27), (18, 28), (24, 26)]


def icons():
    out = {}

    im = canvas(); d = ImageDraw.Draw(im); sun(d, 16, 16, 7); out["clear"] = im
    im = canvas(); d = ImageDraw.Draw(im); sun(d, 11, 11, 5); cloud(d, 8, 26, 22); out["partly-cloudy"] = im
    im = canvas(); d = ImageDraw.Draw(im)
    cloud(d, 3, 18, 18, body=GREY, shade=DARK); cloud(d, 9, 25, 22); out["cloudy"] = im
    im = canvas(); d = ImageDraw.Draw(im); disc(d, 16, 12, 7, (200, 150, 70))
    for y, (x0, x1) in zip((18, 22, 26), ((4, 27), (7, 24), (5, 26))):
        d.line((x0, y, x1, y), fill=HAZE)
    out["haze"] = im
    im = canvas(); d = ImageDraw.Draw(im); cloud(d, 5, 16, 22)
    for y, (x0, x1) in zip((20, 24, 28), ((4, 26), (7, 28), (3, 23))):
        d.line((x0, y, x1, y), fill=FOG)
    out["fog"] = im
    im = canvas(); d = ImageDraw.Draw(im); cloud(d, 5, 17, 22)
    for x, y in [(9, 22), (16, 24), (23, 22), (12, 28), (20, 29)]:
        d.point((x, y), fill=DRIZZLE); d.point((x, y + 1), fill=DRIZZLE)
    out["drizzle"] = im
    im = canvas(); d = ImageDraw.Draw(im); cloud(d, 5, 17, 22); drops(d, RAIN, PRECIP_SPOTS, 4); out["rain"] = im
    im = canvas(); d = ImageDraw.Draw(im); cloud(d, 5, 17, 22)
    flakes(d, SNOW, [(9, 23), (17, 24), (25, 23), (13, 28), (21, 29)]); out["snow"] = im
    im = canvas(); d = ImageDraw.Draw(im); cloud(d, 5, 17, 22)
    drops(d, RAIN, [(9, 22), (21, 22), (16, 27)], 3); flakes(d, ICE, [(15, 22), (24, 27), (9, 28)]); out["sleet"] = im
    im = canvas(); d = ImageDraw.Draw(im); cloud(d, 5, 16, 22, body=GREY, shade=DARK)
    d.polygon([(17, 17), (11, 25), (15, 25), (12, 31), (21, 22), (16, 22), (19, 17)], fill=BOLT)
    drops(d, RAIN, [(8, 20), (25, 20), (24, 26)], 3); out["storm"] = im

    im = canvas(); d = ImageDraw.Draw(im); moon(d, 16, 16, 9); star(d, 5, 6); star(d, 26, 26); star(d, 6, 25)
    out["clear-night"] = im
    im = canvas(); d = ImageDraw.Draw(im); moon(d, 12, 11, 7); star(d, 26, 5); cloud(d, 8, 26, 22)
    out["partly-cloudy-night"] = im
    return out


ORDER = ["clear", "partly-cloudy", "cloudy", "haze", "fog", "drizzle", "rain", "snow", "sleet", "storm"]
NIGHT = ["clear-night", "partly-cloudy-night"]


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main():
    parser = argparse.ArgumentParser(description="Draw the weather icons.")
    parser.add_argument("--preview", help="also write a PNG showing every icon, enlarged")
    args = parser.parse_args()
    drawn = icons()
    lines = [
        "#pragma once",
        "",
        "// Made by tools/make_weather_icons.py; do not edit by hand.",
        "// 32 x 32 RGB565 pictures, one per weather::Icon in order, then the night",
        "// pictures for clear and partly cloudy. 0 is black: off on the panel.",
        "",
        "#include <stdint.h>",
        "",
        "static const int WEATHER_ICON_SIZE = %d;" % N,
        "static const int WEATHER_ICON_NIGHT_CLEAR = %d;" % len(ORDER),
        "static const int WEATHER_ICON_NIGHT_PARTLY_CLOUDY = %d;" % (len(ORDER) + 1),
        "",
        "static const uint16_t WEATHER_ICONS[%d][%d] = {" % (len(ORDER) + len(NIGHT), N * N),
    ]
    for name in ORDER + NIGHT:
        pixels = [rgb565(*drawn[name].getpixel((x, y))) for y in range(N) for x in range(N)]
        lines.append("    // %s" % name)
        lines.append("    {")
        for i in range(0, len(pixels), 16):
            lines.append("        " + ", ".join("0x%04X" % p for p in pixels[i:i + 16]) + ",")
        lines.append("    },")
    lines.append("};")
    path = os.path.join(ROOT, "src", "display", "weather_icons.h")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote", path)

    if args.preview:
        names = ORDER + NIGHT
        scale, gap = 6, 10
        sheet = Image.new("RGB", (len(names) * (N * scale + gap) + gap, N * scale + 2 * gap), (40, 40, 52))
        for i, name in enumerate(names):
            sheet.paste(drawn[name].resize((N * scale, N * scale), Image.NEAREST), (gap + i * (N * scale + gap), gap))
        sheet.save(args.preview)


main()
