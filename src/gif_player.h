#pragma once

#include <Arduino.h>

// Downloads a GIF into RAM (PSRAM when available). The caller frees the buffer.
uint8_t *downloadGif(const String &url, size_t &size);

// Plays a GIF for at least dwellMs, finishing the loop in progress. Artwork
// smaller than the panel is scaled up by a whole number and centred.
bool playGif(uint8_t *data, size_t size, uint32_t dwellMs);
