# Local changes to ESP32-HUB75-MatrixPanel-DMA

A copy of [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
3.1.0 by mrcodetastic, MIT licence (see `LICENSE.txt`), with one function
changed. Everything else is as released, apart from trailing spaces trimmed in
that one file; examples and tools are left out.

## `setBrightnessOE()` in `src/ESP32-HUB75-MatrixPanel-I2S-DMA.cpp`

Marked `PIXEL JUKEBOX PATCH`. To reach a refresh rate, the library shows the
lowest colour bits (up to `lsbMsbTransitionBit`) once each for the same time,
and 3.1.0 gives every bitplane the same OE (output enable) window, so those
bits all weigh the same instead of 1, 2, 4 and so on. At about 100 Hz (normal
mode) that is bits 0 to 2, at about 500 Hz (photo mode) bits 0 to 4: faint
colour components came out up to 13 times too bright, mid tones too bright and
out of order, and colours looked washed out, most of all in photo mode.

1. The buffers that show those planes get OE windows scaled by
   2^(plane - lsbMsbTransitionBit).
2. The first buffer of each row, which shows the previous row's brightest
   plane with this row selected (a faint copy of the row above), is kept dark.

The working, and a model of the light output before and after, are in
`docs/colour.md`.

Worth offering upstream.
