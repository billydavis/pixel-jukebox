# Color on the panel

Why colors looked washed out, especially in photo mode, and what fixed it
(firmware 0.32.0, 2026-10-19).

## How the panel makes a shade

A HUB75 panel's LEDs are only ever on or off. Shades come from how long each
LED is on (binary code modulation): each color channel's 8 bits are shown one
after another, bit 0 for 1 unit of time, bit 1 for 2, bit 2 for 4 ... bit 7 for
128, so a value of 200 is on for 200 units out of 255. Before that, the
display library maps the artwork's 8-bit value through a CIE 1931 lightness
table (gamma), since LEDs give out light linearly and eyes do not see it that
way. That table puts most mid tones in the low bits: 128 becomes 47 (binary
0010 1111).

## What went wrong

Showing bit 7 for 128 units means sending the row many times, and the time that
takes caps the refresh rate. To refresh faster, the library (ESP32-HUB75-
MatrixPanel-DMA 3.1.0) shows every bit up to a "transition bit" `t` once, for
the same time, and only weights the bits above it. It should then shorten the
on-window (the OE pulse) of those low bits to restore their weights; 3.1.0
gives every bit the same window. So bits 0 to `t` all weigh the same.

| | `t` | Real weights, bits 0 to 7 |
|---|---|---|
| Should be | | 1, 2, 4, 8, 16, 32, 64, 128 |
| Normal mode, about 100 Hz | 2 | 1, 1, 1, 2, 4, 8, 16, 32 |
| Photo mode, about 520 Hz | 4 | 1, 1, 1, 1, 1, 2, 4, 8 |

The faint parts of every color came out far too bright, and the brightness
scale even ran backwards in places (31 brighter than 32). A deep red with a
little green in it had that green blown up, so it looked chalky and pale.

A second, smaller fault: while each row's first buffer is sent, the LEDs still
hold the previous row's brightest bit, but this row is already selected and its
OE window is open: a faint copy of the row above on every row.

`tools/colour_model.py` works out the light for every value, before and after.
At brightness 50:

| | Photo now | Photo fixed | Normal now | Normal fixed | Ideal |
|---|---|---|---|---|---|
| Value 64 | 16.7 % | 4.5 % | 6.2 % | 4.3 % | 4.3 % |
| Value 96 | 16.7 % | 10.4 % | 10.9 % | 9.9 % | 9.8 % |
| Green in a (200, 40, 0) red | 22.2 % | 4.4 % | 5.9 % | 3.6 % | 3.6 % |
| Ghost of a white row above | 5.6 % | 0 % | 1.6 % | 0 % | 0 % |
| Steps that go backwards | 46 | 1 | 28 | 1 | 0 |

## The fix

In the library's `setBrightnessOE()` (vendored in
`lib/ESP32-HUB75-MatrixPanel-DMA`, see its `PATCHES.md`):

1. The buffers showing bits below `t` get on-windows scaled by 2^(bit - t).
   They are only used once per row, so nothing else changes.
2. Each row's first buffer stays dark, removing the ghost. The top bit loses
   one of its showings, which is the one step that still goes backwards.

At low brightness the windows are only a few pixel clocks wide, so the lowest
bits round to the nearest clock; the model includes that rounding.

## Why not red, green and blue sliders

The error depended on each pixel's value, not on the channel, so a fixed
per-channel gain could not undo it; it would only tint everything. Once the
timing is right, a white-balance adjustment is still possible if whites look
tinted on this panel (LED panels often run slightly blue), but that is a
separate, smaller question.
