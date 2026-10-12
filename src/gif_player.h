#pragma once

#include <Arduino.h>

// Downloads a GIF into RAM (PSRAM when available). The caller frees the buffer.
uint8_t *downloadGif(const String &url, size_t &size);

// Opens a GIF held in RAM and clears the panel. Artwork smaller than the panel
// is scaled up by a whole number and centred. The buffer must outlive gifClose().
bool gifOpen(uint8_t *data, size_t size);

// Draws the next frame and reports how long to show it. Returns 1 when more
// frames follow, 0 after the last frame of the loop, and a negative value on
// a decode error.
int gifPlayFrame(int &frameMs);

// Rewinds to the first frame.
void gifRestart();

void gifClose();
