"""Make the starter art: the 64x64 GIFs built into the firmware (backlog #28).

    python -I tools/make_starter_art.py <packs folder> [--preview preview.html]

Writes assets/starter/NN-name.gif, a one-frame still of each in
assets/starter/stills/ (for the control page's thumbnails: the web server is
slow, and 28 animations are 1.1 MB), and assets/starter/manifest.json. The
build packs them into the firmware (tools/stage_starter_art.py). The GIFs are kept
in git, so this only needs running to change them.

The packs are not kept in git. Download each into <packs folder> and unzip it
to the folder named below. All are CC0 1.0; the evidence is in
docs/starter-art.md and the credits in CREDITS.md.

    pixel-adventure-1/        https://pixelfrog-assets.itch.io/pixel-adventure-1
    dungeontileset-ii/        https://0x72.itch.io/dungeontileset-ii (v1.7)
    sunny-land/               https://ansimuz.itch.io/sunny-land-pixel-game-art
                              (Sunny-land-files.zip only)
    kenney_pixel-platformer/  https://kenney.nl/assets/pixel-platformer
    kenney_tiny-dungeon/      https://kenney.nl/assets/tiny-dungeon

The pieces marked "code" are drawn here and need no pack.
"""
import argparse
import base64
import colorsys
import glob
import html
import json
import math
import os
import random

from PIL import Image, ImageDraw

parser = argparse.ArgumentParser(description="Make the starter art GIFs.")
parser.add_argument("packs", help="folder holding the unzipped packs")
parser.add_argument("--preview", help="also write an HTML page showing every piece")
args = parser.parse_args()

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = args.packs
OUT = os.path.join(ROOT, "assets", "starter")
os.makedirs(OUT, exist_ok=True)
N = 64
PA = os.path.join(SRC, "pixel-adventure-1", "Free")
DT = os.path.join(SRC, "dungeontileset-ii", "0x72_DungeonTilesetII_v1.7", "frames")
SL = os.path.join(SRC, "sunny-land", "Sunny-land-files", "Assets")
KP = os.path.join(SRC, "kenney_pixel-platformer", "Tilemap")
KD = os.path.join(SRC, "kenney_tiny-dungeon", "Tilemap")

# Who made each source, as shown on the control page, and its licence.
SOURCES = {
    "pa": ("Pixel Frog, Pixel Adventure 1", "CC0 1.0"),
    "dt": ("0x72, DungeonTileset II", "CC0 1.0"),
    "sl": ("ansimuz, Sunny Land", "CC0 1.0"),
    "kp": ("Kenney, Pixel Platformer", "CC0 1.0"),
    "kd": ("Kenney, Tiny Dungeon", "CC0 1.0"),
    "code": ("Pixel Jukebox", "Apache 2.0"),
}

pieces = []


def load(path):
    return Image.open(path).convert("RGBA")


def strip(path, w):
    im = load(path)
    return [im.crop((i * w, 0, i * w + w, im.height)) for i in range(im.width // w)]


def dt(name):
    files = sorted(glob.glob(os.path.join(DT, name + "_f*.png")))
    assert files, name
    return [load(f) for f in files]


def tiles(path, t):
    im = load(path)
    cols = im.width // t
    return lambda i: im.crop(((i % cols) * t, (i // cols) * t, (i % cols) * t + t, (i // cols) * t + t))


def canvas(colour=(0, 0, 0, 255)):
    return Image.new("RGBA", (N, N), colour)


def put(base, sprite, x, y, flip=False):
    if flip:
        sprite = sprite.transpose(Image.FLIP_LEFT_RIGHT)
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    layer.paste(sprite, (int(x), int(y)))
    base.alpha_composite(layer)


def scaled(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


def add(key, title, src, note, frames, ms):
    pieces.append((key, title, src, note, frames, ms))


# ---------------------------------------------------------------- Pixel Adventure
terrain = load(os.path.join(PA, "Terrain", "Terrain (16x16).png"))
GROUND = {  # left-top corner of each 3x3 grass/dirt block
    "green": (96, 0),
    "orange": (96, 64),
    "pink": (96, 128),
}


def ground_row(c, kind, y, ox=0):
    gx, gy = GROUND[kind]
    top = terrain.crop((gx + 16, gy, gx + 32, gy + 16))
    mid = terrain.crop((gx + 16, gy + 16, gx + 32, gy + 32))
    for x in range(-16, N + 16, 16):
        xx = x - (ox % 16)
        c.paste(top, (xx, y), top)
        for yy in range(y + 16, N, 16):
            c.paste(mid, (xx, yy), mid)


heroes = [("Ninja Frog", "Blue", "green"), ("Mask Dude", "Yellow", "orange"),
          ("Pink Man", "Purple", "pink"), ("Virtual Guy", "Green", "green")]
for name, bg, kind in heroes:
    run = strip(os.path.join(PA, "Main Characters", name, "Run (32x32).png"), 32)
    tile = load(os.path.join(PA, "Background", bg + ".png"))
    frames = []
    for i in range(48):  # 48 = 4 run cycles; background scrolls 64 px, ground 96 px
        c = canvas()
        off = (i * 4 // 3)
        for y in (-64, 0, 64):
            c.paste(tile, (0, y + off % 64), tile)
        ground_row(c, kind, 44, ox=i * 2)
        put(c, run[i % len(run)], 16, 13)
        frames.append(c)
    add("pa-" + name.lower().replace(" ", "-"), name + " runs", "pa",
        "Run cycle at the pack's 20 fps over a scrolling tiled sky", frames, 50)

# Four heroes idling, 2x2
idles = [strip(os.path.join(PA, "Main Characters", h[0], "Idle (32x32).png"), 32) for h in heroes]
tile = load(os.path.join(PA, "Background", "Gray.png"))
frames = []
for i in range(11):
    c = canvas()
    c.paste(tile, (0, 0), tile)
    for k, idle in enumerate(idles):
        put(c, idle[(i + k * 3) % len(idle)], (k % 2) * 32, (k // 2) * 32)
    frames.append(c)
add("pa-lineup", "The four heroes", "pa", "All four idle animations at once", frames, 50)

# Fruit parade, 2x2
fruits = [strip(os.path.join(PA, "Items", "Fruits", f + ".png"), 32) for f in ("Apple", "Bananas", "Cherries", "Melon")]
tile = load(os.path.join(PA, "Background", "Pink.png"))
frames = []
for i in range(17):
    c = canvas()
    c.paste(tile, (0, 0), tile)
    for k, fr in enumerate(fruits):
        put(c, fr[(i + k * 4) % 17], (k % 2) * 32, (k // 2) * 32)
    frames.append(c)
add("pa-fruit", "Fruit bowl", "pa", "Four spinning fruits, 17 frames each", frames, 50)

# Fruit, big: one pineapple scaled x2
fr = strip(os.path.join(PA, "Items", "Fruits", "Pineapple.png"), 32)
tile = load(os.path.join(PA, "Background", "Brown.png"))
frames = []
for i in range(17):
    c = canvas()
    c.paste(tile, (0, 0), tile)
    put(c, scaled(fr[i], 2), 0, 0)
    frames.append(c)
add("pa-pineapple", "Pineapple, doubled", "pa", "A 32x32 sprite scaled x2 with sharp pixels", frames, 50)

# Saw trap
saw = strip(os.path.join(PA, "Traps", "Saw", "On (38x38).png"), 38)
chain = load(os.path.join(PA, "Traps", "Saw", "Chain.png"))
tile = load(os.path.join(PA, "Background", "Gray.png"))
frames = []
for i in range(32):
    c = canvas()
    c.paste(tile, (0, 0), tile)
    for x in range(0, N, 8):
        c.paste(chain, (x, 28), chain)
    x = 13 + 20 * math.sin(i / 32 * 2 * math.pi)
    put(c, saw[i % len(saw)], x, 13)
    frames.append(c)
add("pa-saw", "Saw on a chain", "pa", "A spinning trap swinging along its chain", frames, 50)

# ---------------------------------------------------------------- DungeonTileset II
floors = [load(os.path.join(DT, "floor_%d.png" % k)) for k in range(1, 9)]
random.seed(7)
floor_map = [[random.choice([0, 0, 0, 1, 2, 3, 4, 5, 6, 7]) for _ in range(4)] for _ in range(4)]


def dungeon_room(top_wall=True):
    c = canvas((34, 32, 52, 255))
    for y in range(4):
        for x in range(4):
            c.paste(floors[floor_map[y][x]], (x * 16, y * 16))
    if top_wall:
        wt, wm = load(os.path.join(DT, "wall_top_mid.png")), load(os.path.join(DT, "wall_mid.png"))
        for x in range(0, N, 16):
            c.paste(wt, (x, 0), wt)
            c.paste(wm, (x, 16), wm)
    return c


fount_mid = dt("wall_fountain_mid_red_anim")
fount_basin = dt("wall_fountain_basin_red_anim")
fount_top = load(os.path.join(DT, "wall_fountain_top_1.png"))
knight = dt("knight_m_idle_anim")
demon = dt("big_demon_idle_anim")
frames = []
for i in range(12):
    c = dungeon_room()
    put(c, fount_top, 24, 0)
    put(c, fount_mid[i % 3], 24, 16)
    put(c, fount_basin[i % 3], 24, 32)
    put(c, knight[i % 4], 4, 32)
    put(c, demon[i % 4], 34, 24, flip=True)
    frames.append(c)
add("dt-fountain", "Knight meets demon", "dt", "Lava fountain, knight and big demon, all animated", frames, 110)

lineup = ["knight_f_idle_anim", "elf_m_idle_anim", "wizzard_f_idle_anim", "lizard_m_idle_anim"]
sprites = [dt(n) for n in lineup]
frames = []
for i in range(8):
    c = dungeon_room()
    for k, s in enumerate(sprites):
        put(c, s[(i + k) % 4], k * 16, 26)
    frames.append(c)
add("dt-heroes", "Party of four", "dt", "Knight, elf, wizard and lizard idling", frames, 120)

monsters = ["big_demon_run_anim", "ogre_run_anim"]
sprites = [dt(n) for n in monsters]
frames = []
for i in range(8):
    c = dungeon_room(top_wall=False)
    put(c, sprites[0][i % 4], 0, 14)
    put(c, sprites[1][(i + 2) % 4], 32, 14, flip=True)
    frames.append(c)
add("dt-big", "Big trouble", "dt", "Demon and ogre running in place", frames, 100)

mimic = dt("chest_mimic_open_anim")
coin = dt("coin_anim")
frames = []
seq = [0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 1, 0]
for i in range(12):
    c = dungeon_room(top_wall=False)
    put(c, scaled(mimic[seq[i]], 2), 16, 20)
    for k in range(4):
        put(c, coin[(i + k) % 4], 4 + k * 16, 4)
    frames.append(c)
add("dt-mimic", "The mimic", "dt", "A chest that bites, scaled x2, with spinning coins", frames, 120)

small = ["goblin_run_anim", "imp_run_anim", "skelet_run_anim", "tiny_zombie_run_anim",
         "chort_run_anim", "wogol_run_anim", "masked_orc_run_anim", "pumpkin_dude_run_anim"]
sprites = [dt(n) for n in small]
frames = []
for i in range(8):
    c = dungeon_room(top_wall=False)
    for k, s in enumerate(sprites):
        f = s[(i + k) % len(s)]
        put(c, f, (k % 4) * 16, (k // 4) * 32 + 32 - f.height)
    frames.append(c)
add("dt-crowd", "Monster crowd", "dt", "Eight small monsters running", frames, 100)

# ---------------------------------------------------------------- Sunny Land
back = load(os.path.join(SL, "environment", "Background", "back.png"))
middle = load(os.path.join(SL, "environment", "Background", "middle.png"))
fox = [load(f) for f in sorted(glob.glob(os.path.join(SL, "Characters", "Foxy", "run", "sprites", "*.png")))]
block = load(os.path.join(SL, "environment", "Props", "block.png"))


def sunny_bg(i, back_speed, mid_speed, back_y=70):
    c = canvas()
    bx = int(i * back_speed) % back.width
    for x in (-bx, back.width - bx):
        c.paste(back, (x, -back_y))
    if mid_speed is not None:
        mx = int(i * mid_speed) % (middle.width + 40)
        c.paste(middle, (N - mx + 20, 10), middle)
        c.paste(middle, (N - mx + 20 + middle.width + 40, 10), middle)
    return c


frames = []
for i in range(48):
    c = sunny_bg(i, 0.5, 1)
    for x in range(-16, N + 16, 16):
        c.paste(block, (x - (i * 2) % 16, 48))
    put(c, fox[i % len(fox)], 14, 17)
    frames.append(c)
add("sl-fox", "Fox on the run", "sl", "Three-layer parallax: sky, island, ground", frames, 80)

eagle = [load(f) for f in sorted(glob.glob(os.path.join(SL, "Characters", "eagle", "Sprites", "attack", "*.png")))]
frames = []
for i in range(32):
    c = sunny_bg(i, 1, None, back_y=40)
    put(c, eagle[i % 4], 12, 10 + 4 * math.sin(i / 32 * 2 * math.pi))
    frames.append(c)
add("sl-eagle", "Eagle over the sea", "sl", "Flapping eagle over moving clouds and sea", frames, 90)

frog_idle = [load(f) for f in sorted(glob.glob(os.path.join(SL, "Characters", "frog", "Sprites", "idle", "*.png")))]
frog_jump = [load(f) for f in sorted(glob.glob(os.path.join(SL, "Characters", "frog", "Sprites", "jump", "*.png")))]
opossum = [load(f) for f in sorted(glob.glob(os.path.join(SL, "Characters", "Opossum", "opossum", "*.png")))]
frames = []
for i in range(48):
    c = sunny_bg(0, 0, None, back_y=60)
    for x in range(0, N, 16):
        c.paste(block, (x, 52))
    t = i % 24
    if t < 12:
        f, y = frog_idle[(t // 3) % 4], 20
    else:
        p = (t - 12) / 12
        f, y = frog_jump[0 if p < 0.5 else 1], 20 - 18 * math.sin(p * math.pi)
    put(c, f, 2, y)
    put(c, opossum[i % 6], N - (i * 2) % (N + 36), 24, flip=False)
    frames.append(c)
add("sl-critters", "Frog and opossum", "sl", "A hopping frog while an opossum scurries past", frames, 90)

gems = [load(f) for f in sorted(glob.glob(os.path.join(SL, "Misc", "Sunnyland items", "Sprites", "gem", "*.png")))]
cherries = [load(f) for f in sorted(glob.glob(os.path.join(SL, "Misc", "Sunnyland items", "Sprites", "cherry", "*.png")))]
frames = []
for i in range(max(len(gems), len(cherries)) * 2):
    c = sunny_bg(i, 0.5, None, back_y=20)
    gem, cherry = scaled(gems[i % len(gems)], 2), scaled(cherries[i % len(cherries)], 2)
    put(c, gem, 2, 2)
    put(c, cherry, N - cherry.width - 1, N - cherry.height - 1)
    frames.append(c)
add("sl-items", "Gem and cherry", "sl", "Pickup items scaled x2 over drifting clouds", frames, 100)

# ---------------------------------------------------------------- Kenney
kt = tiles(os.path.join(KP, "tilemap_packed.png"), 18)
kb = tiles(os.path.join(KP, "tilemap-backgrounds_packed.png"), 24)
kc = tiles(os.path.join(KP, "tilemap-characters_packed.png"), 24)
frames = []
for i in range(24):
    c = canvas()
    for x in range(0, N + 24, 24):
        c.paste(kb(0), (x, 0))
        c.paste(kb(9), (x, 24))
        c.paste(kb(16), (x, 48))
    for x in range(0, N + 18, 18):
        c.paste(kt(2), (x - 4, 46), kt(2))
    c.paste(kt(125), (44, 28), kt(125))
    c.paste(kt(146), (2, 28), kt(146))
    put(c, kc(0 if (i // 4) % 2 == 0 else 1), 18, 22 - (6 * abs(math.sin(i / 24 * 2 * math.pi))))
    put(c, kt(151 if (i // 3) % 2 == 0 else 152), 40, 6 + 2 * math.sin(i / 24 * 2 * math.pi))
    frames.append(c)
add("kp-meadow", "Meadow hop", "kp", "Small platformer scene with a bouncing alien and coin", frames, 80)

kdt = tiles(os.path.join(KD, "tilemap_packed.png"), 16)
frames = []
for i in range(16):
    c = canvas()
    for x in range(4):
        c.paste(kdt(40), (x * 16, 0))
        for y in range(1, 4):
            c.paste(kdt(48 + (x + y) % 4), (x * 16, y * 16))
    c.paste(kdt(29), (16, 0), kdt(29))
    c.paste(kdt(89 if i < 8 else 91), (40, 40), kdt(89 if i < 8 else 91))
    for k, (t, x, y) in enumerate([(84, 4, 22), (96, 22, 30), (99, 4, 44), (110, 44, 18)]):
        put(c, kdt(t), x, y - (1 if (i + k * 2) % 8 < 4 else 0))
    frames.append(c)
add("kd-room", "Tiny dungeon room", "kd", "Wizard, knight and friends; a chest pops open", frames, 120)

# ---------------------------------------------------------------- Drawn by code
def code(key, title, note, fn, count, ms):
    frames = []
    for i in range(count):
        im = Image.new("RGB", (N, N))
        px = im.load()
        fn(i, count, px, im)
        frames.append(im.convert("RGBA"))
    add(key, title, "code", note, frames, ms)


def hsv(h, s, v):
    r, g, b = colorsys.hsv_to_rgb(h % 1, s, v)
    return int(r * 255), int(g * 255), int(b * 255)


def plasma(i, n, px, im):
    t = i / n * 2 * math.pi
    for y in range(N):
        for x in range(N):
            v = (math.sin(x / 7 + t) + math.sin(y / 9 - t) + math.sin((x + y) / 11 + t) +
                 math.sin(math.hypot(x - 32, y - 32) / 6 - 2 * t))
            px[x, y] = hsv(v / 8 + i / n, 0.85, 1)


def sunset(i, n, px, im):
    t = i / n * 2 * math.pi
    sun_y = 40 + 4 * math.sin(t)
    for y in range(N):
        k = y / (N - 1)
        r = int(40 + 215 * k ** 0.7)
        g = int(20 + 110 * k ** 1.6)
        b = int(90 - 60 * k)
        for x in range(N):
            px[x, y] = (r, g, b)
    d = ImageDraw.Draw(im)
    d.ellipse((20, sun_y - 12, 44, sun_y + 12), fill=(255, 214, 120))
    for y in range(46, N):
        k = (y - 46) / 18
        for x in range(N):
            shimmer = 0.5 + 0.5 * math.sin(x / 3 + y * 1.3 + t * 2)
            if abs(x - 32) < 14 * (1 - k * 0.4) and shimmer > 0.55:
                px[x, y] = (230, 150, 90)
            else:
                px[x, y] = (int(30 + 30 * k), int(20 + 20 * k), int(60 + 20 * k))


def wheel(i, n, px, im):
    a0 = i / n
    for y in range(N):
        for x in range(N):
            dx, dy = x - 31.5, y - 31.5
            r = math.hypot(dx, dy)
            if r > 31:
                px[x, y] = (0, 0, 0)
            else:
                px[x, y] = hsv(math.atan2(dy, dx) / (2 * math.pi) + a0, min(1, r / 31), 1)


def cube(i, n, px, im):
    t = i / n * 2 * math.pi
    pts = [(x, y, z) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)]
    edges = [(a, b) for a in range(8) for b in range(a + 1, 8) if sum(pts[a][k] != pts[b][k] for k in range(3)) == 1]
    proj = []
    for x, y, z in pts:
        x, z = x * math.cos(t) - z * math.sin(t), x * math.sin(t) + z * math.cos(t)
        y, z = y * math.cos(t / 2) - z * math.sin(t / 2), y * math.sin(t / 2) + z * math.cos(t / 2)
        f = 70 / (z + 4)
        proj.append((32 + x * f * 0.55, 32 + y * f * 0.55))
    d = ImageDraw.Draw(im)
    for k, (a, b) in enumerate(edges):
        d.line([proj[a], proj[b]], fill=hsv(k / 12 + i / n, 0.8, 1), width=1)


def ripples(i, n, px, im):
    t = i / n * 2 * math.pi
    for y in range(N):
        for x in range(N):
            r1 = math.hypot(x - 20, y - 24)
            r2 = math.hypot(x - 46, y - 42)
            v = math.sin(r1 / 2.2 - t * 2) + math.sin(r2 / 2.6 - t * 2)
            k = (v + 2) / 4
            px[x, y] = (int(10 + 40 * k), int(60 + 120 * k), int(120 + 135 * k))


life_state = {}


def life(i, n, px, im):
    g = life_state.get("g")
    if g is None or i == 0:
        random.seed(3)
        g = [[random.random() < 0.3 for _ in range(N)] for _ in range(N)]
        life_state["age"] = [[0] * N for _ in range(N)]
    age = life_state["age"]
    for y in range(N):
        for x in range(N):
            if g[y][x]:
                px[x, y] = hsv(0.33 + min(age[y][x], 30) / 60, 0.9, 1)
            else:
                px[x, y] = (0, 0, 0)
    ng = [[False] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            s = sum(g[(y + dy) % N][(x + dx) % N] for dy in (-1, 0, 1) for dx in (-1, 0, 1)) - g[y][x]
            ng[y][x] = s == 3 or (g[y][x] and s == 2)
            age[y][x] = age[y][x] + 1 if ng[y][x] and g[y][x] else 0
    life_state["g"] = ng


FONT = {  # 3x5
    "P": "111101111100100", "I": "111010010010111", "X": "101101010101101", "E": "111100110100111",
    "L": "100100100100111", "J": "001001001101111", "U": "101101101101111", "K": "101101110101101",
    "B": "110101110101110", "O": "111101101101111", " ": "000000000000000",
}


def text(px, s, x0, y0, colour_fn):
    for k, ch in enumerate(s):
        bits = FONT[ch]
        for r in range(5):
            for c in range(3):
                if bits[r * 3 + c] == "1":
                    x, y = x0 + k * 4 + c, y0 + r
                    if 0 <= x < N and 0 <= y < N:
                        px[x, y] = colour_fn(x, y)


def logo(i, n, px, im):
    t = i / n
    for y in range(N):
        for x in range(N):
            px[x, y] = (8, 6, 18)
    for k in range(5):
        h = 4 + 10 * abs(math.sin((t * 2 + k * 0.17) * math.pi * 2))
        for y in range(int(56 - h), 56):
            for x in range(12 + k * 8, 18 + k * 8):
                px[x, y] = hsv(k / 6 + t, 0.8, 1)
    text(px, "PIXEL", 22, 10, lambda x, y: hsv(x / 64 + t, 0.6, 1))
    text(px, "JUKEBOX", 18, 20, lambda x, y: hsv(x / 64 + t + 0.5, 0.6, 1))


def stars(i, n, px, im):
    random.seed(11)
    st = [(random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(0.1, 1)) for _ in range(160)]
    for y in range(N):
        for x in range(N):
            px[x, y] = (0, 0, 8)
    for sx, sy, sz in st:
        z = (sz - i / n) % 1 + 0.05
        x, y = 32 + sx / z * 12, 32 + sy / z * 12
        if 0 <= x < N and 0 <= y < N:
            b = int(255 * min(1, (1 - z) * 1.3))
            px[int(x), int(y)] = (b, b, min(255, b + 30))


def ramps(i, n, px, im):
    t = i / n
    for y in range(N):
        band = y // 8
        for x in range(N):
            v = ((x + i * 2) % N) / (N - 1)
            px[x, y] = hsv(band / 8 + t * 0.25, 1, v ** 1.2)


code("code-plasma", "Plasma", "Full-panel colour motion", plasma, 48, 60)
code("code-sunset", "Sunset", "Smooth gradient and shimmer, shows the colour fix", sunset, 32, 90)
code("code-wheel", "Colour wheel", "Every hue and saturation, turning", wheel, 36, 60)
code("code-cube", "Spinning cube", "3D wireframe, rainbow edges", cube, 60, 50)
code("code-ripples", "Ripples", "Two interfering water ripples", ripples, 30, 60)
code("code-life", "Game of Life", "Conway's cellular automaton, coloured by age", life, 120, 80)
code("code-logo", "Pixel Jukebox", "Logo with a bouncing equaliser", logo, 40, 60)
code("code-stars", "Starfield", "Flying through stars", stars, 40, 50)
code("code-ramps", "Glow bands", "Eight hues fading to black, sliding", ramps, 32, 60)


# ---------------------------------------------------------------- write
def save_gif(frames, ms, path):
    pal = [f.convert("RGB") for f in frames]
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=ms, loop=0, disposal=1, optimize=False)


STILLS = os.path.join(OUT, "stills")
os.makedirs(STILLS, exist_ok=True)
for old in glob.glob(os.path.join(OUT, "*.gif")) + glob.glob(os.path.join(STILLS, "*.gif")):
    os.remove(old)
manifest = []
for idx, (key, title, src, note, frames, ms) in enumerate(pieces, 1):
    name = "%02d-%s.gif" % (idx, key)
    save_gif(frames, ms, os.path.join(OUT, name))
    # A third of the way in: past any fade-in, and representative of the motion.
    frames[len(frames) // 3].convert("RGB").quantize(256).save(os.path.join(STILLS, name))
    who, lic = SOURCES[src]
    manifest.append({"file": name, "still": "stills/" + name, "title": title, "credit": who, "licence": lic,
                     "note": note})
with open(os.path.join(OUT, "manifest.json"), "w", encoding="utf-8", newline="\n") as f:
    json.dump(manifest, f, indent=2)
    f.write("\n")
total = sum(os.path.getsize(os.path.join(OUT, m["file"])) for m in manifest)
print(len(manifest), "pieces,", round(total / 1024), "KB in", OUT)

if args.preview:
    figures = []
    for idx, m in enumerate(manifest, 1):
        data = base64.b64encode(open(os.path.join(OUT, m["file"]), "rb").read()).decode()
        figures.append(
            '<figure><span class="n">%d</span><img src="data:image/gif;base64,%s" alt="" width="192" height="192">'
            "<figcaption><b>%s</b><span>%s</span><small>%s &middot; %s</small></figcaption></figure>"
            % (idx, data, html.escape(m["title"]), html.escape(m["note"]), html.escape(m["credit"]),
               html.escape(m["licence"])))
    page = """<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Starter Art</title>
<style>
:root{--bg:#14121c;--fg:#ece8f4;--dim:#9a94ab}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,sans-serif;padding:24px 16px}
h1{font-size:24px;margin:0 0 16px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:18px}
figure{margin:0;position:relative}
img{display:block;width:100%%;max-width:256px;height:auto;aspect-ratio:1;image-rendering:pixelated;background:#000;border-radius:6px}
.n{position:absolute;top:6px;left:6px;background:#000c;color:#fff;font:700 13px/1 ui-monospace,monospace;padding:4px 6px;border-radius:4px}
figcaption{display:flex;flex-direction:column;gap:2px;margin-top:6px}
figcaption small{color:var(--dim)}
</style></head><body><h1>Starter art</h1><div class="grid">
%s
</div></body></html>""" % "\n".join(figures)
    with open(args.preview, "w", encoding="utf-8") as f:
        f.write(page)
