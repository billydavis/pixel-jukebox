"""Light out of one LED channel for each 8-bit colour value, as the HUB75
library drives it, with and without the fixes in lib/ESP32-HUB75-MatrixPanel-DMA
(PATCHES.md). The reasoning is in docs/color.md.

    python tools/colour_model.py

Model: 8 bitplanes. While a buffer is shifted in, the LEDs show the plane
latched at the end of the buffer before it. Per row the buffer order is
p0..p7, then for each i > t (lsbMsbTransitionBit), 2^(i-t-1) sweeps of
p_i..p7. Each buffer has an OE (output enable) window of `active` pixel clocks.
Fix A: buffers 1..t (showing planes 0..t-1) get windows scaled by 2^(b-t).
Fix B: buffer 0 (showing the previous row's top plane) stays dark.
"""
import re
from pathlib import Path

src = (Path(__file__).resolve().parent.parent / "lib/ESP32-HUB75-MatrixPanel-DMA/src/cie_luts.h").read_text()
block = src[src.index("lumConvTab_8bit[256]"):]
lut = [int(x) for x in re.findall(r"\d+", block[block.index("{"):block.index("}")])][:256]
D = 8


def segments(t):
    segs = list(range(D))
    for i in range(t + 1, D):
        for _ in range(1 << (i - t - 1)):
            segs += list(range(i, D))
    return segs


def light(v, t, active, fixA, fixB, prev_row_v=0):
    g = lut[v]
    gp = lut[prev_row_v]
    segs = segments(t)
    total = 0.0
    for k, buf in enumerate(segs):
        window = active
        if fixA and 1 <= buf <= t:
            window = round(active * 2 ** ((buf - 1) - t))
        if k == 0:
            if fixB:
                continue
            shown = (gp >> (D - 1)) & 1  # the row above's top plane, on this row
        else:
            shown = (g >> segs[k - 1]) & 1
        total += shown * window
    return total


def report(name, t, blank, brightness):
    active = max(1, ((64 - 2 * blank) * brightness) // 255)
    print(f"\n{name}: t={t}, brightness {brightness} -> on-window {active} clocks")
    for label, a, b in (("before", False, False), ("after", True, True)):
        top = light(255, t, active, a, b)
        curve = [light(v, t, active, a, b) / top for v in range(256)]
        ideal = [lut[v] / 255 for v in range(256)]
        backwards = sum(1 for v in range(1, 256) if curve[v] < curve[v - 1] - 1e-9)
        tint = curve[40] / curve[200]
        tint_ideal = ideal[40] / ideal[200]
        ghost = light(0, t, active, a, b, prev_row_v=255) / top
        samples = " ".join(f"{v}:{curve[v]*100:.1f}%" for v in (16, 40, 64, 96, 128, 192))
        print(f"  {label:6} steps that go backwards {backwards:3}   green in a (200,40,0) red "
              f"{tint*100:5.1f}% (ideal {tint_ideal*100:.1f}%)   ghost of a white row above {ghost*100:4.1f}%")
        print(f"         light out: {samples}")
    print("  ideal  " + " ".join(f"{v}:{lut[v]/255*100:.1f}%" for v in (16, 40, 64, 96, 128, 192)))


for brightness in (50, 255):
    report("NORMAL (8 MHz, blanking 4)", 2, 4, brightness)
    report("PHOTO (16 MHz, blanking 8)", 4, 8, brightness)
