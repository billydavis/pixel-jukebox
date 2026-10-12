#include "gif_player.h"

#include <AnimatedGIF.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "pins.h"

static const int MAX_GIF_BYTES = 4 * 1024 * 1024;
static const uint32_t DOWNLOAD_STALL_MS = 10000;
static const int MIN_FRAME_MS = 20;
static const int DEFAULT_FRAME_MS = 100; // what browsers use for near-zero delays

// AnimatedGIF reads the header in fixed-size chunks and rejects files too small
// to fill them, which rules out most tiny artwork. Files under SMALL_GIF_BYTES
// get a comment block of zeros inserted after the header to make up the size.
static const size_t SMALL_GIF_BYTES = 255 + 3 * 256 + 32;
static const int PAD_BLOCKS = 4;
static const size_t PAD_BYTES = 2 + PAD_BLOCKS * 256 + 1;

static AnimatedGIF gif; // large (LZW buffers), so kept off the stack
static int scale = 1;
static int offsetX = 0;
static int offsetY = 0;

// The buffer must have PAD_BYTES of spare room after the file.
static void padSmallGif(uint8_t *data, size_t &size) {
  if (size >= SMALL_GIF_BYTES || size < 13 || memcmp(data, "GIF8", 4) != 0) {
    return;
  }
  uint8_t flags = data[10];
  size_t headerBytes = 13 + ((flags & 0x80) ? 3 * (1 << ((flags & 7) + 1)) : 0);
  if (headerBytes >= size) {
    return;
  }

  uint8_t *pad = data + headerBytes;
  memmove(pad + PAD_BYTES, pad, size - headerBytes);
  memset(pad, 0, PAD_BYTES); // the final zero ends the comment
  pad[0] = 0x21;             // extension
  pad[1] = 0xFE;             // comment
  for (int block = 0; block < PAD_BLOCKS; block++) {
    pad[2 + block * 256] = 255; // sub-block length
  }
  size += PAD_BYTES;
}

uint8_t *downloadGif(const String &url, size_t &size) {
  WiFiClient client;
  HTTPClient http;
  http.setUserAgent(USER_AGENT);
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    return nullptr;
  }

  int status = http.GET();
  int length = http.getSize();
  if (status != 200 || length <= 0 || length > MAX_GIF_BYTES) {
    Serial.printf("Download refused: HTTP %d, %d bytes\n", status, length);
    http.end();
    return nullptr;
  }

  uint8_t *buffer = (uint8_t *)ps_malloc(length + PAD_BYTES);
  if (!buffer) {
    buffer = (uint8_t *)malloc(length + PAD_BYTES);
  }
  if (!buffer) {
    http.end();
    return nullptr;
  }

  WiFiClient *stream = http.getStreamPtr();
  int received = 0;
  uint32_t lastData = millis();
  while (received < length && millis() - lastData < DOWNLOAD_STALL_MS) {
    // Only read what has arrived: readBytes() busy-waits, which starves the
    // idle task and trips the watchdog when this runs in the loader task.
    int available = stream->available();
    if (available <= 0) {
      if (!stream->connected()) {
        break;
      }
      delay(5);
      continue;
    }
    int count = stream->read(buffer + received, min(length - received, available));
    if (count > 0) {
      received += count;
      lastData = millis();
    }
  }
  http.end();

  if (received < length) {
    Serial.printf("Download stalled at %d of %d bytes\n", received, length);
    free(buffer);
    return nullptr;
  }
  size = length;
  padSmallGif(buffer, size);
  return buffer;
}

static void drawLine(GIFDRAW *draw) {
  int y = offsetY + (draw->iY + draw->y) * scale;
  if (y < 0 || y + scale > PANEL_RES_Y) {
    return;
  }
  for (int x = 0; x < draw->iWidth; x++) {
    uint8_t index = draw->pPixels[x];
    uint16_t colour;
    if (draw->ucHasTransparency && index == draw->ucTransparent) {
      if (draw->ucDisposalMethod != 2) {
        continue; // keep the previous frame's pixel
      }
      colour = 0; // "restore to background"
    } else {
      colour = draw->pPalette[index];
    }

    int px = offsetX + (draw->iX + x) * scale;
    if (px < 0 || px + scale > PANEL_RES_X) {
      continue;
    }
    if (scale == 1) {
      display->drawPixel(px, y, colour);
    } else {
      display->fillRect(px, y, scale, scale, colour);
    }
  }
}

bool gifOpen(uint8_t *data, size_t size) {
  gif.begin(GIF_PALETTE_RGB565_LE);
  if (!gif.open(data, size, drawLine)) {
    Serial.printf("GIF open failed: error %d\n", gif.getLastError());
    return false;
  }

  int width = gif.getCanvasWidth();
  int height = gif.getCanvasHeight();
  if (width <= 0 || height <= 0) {
    gif.close();
    return false;
  }
  scale = max(1, min(PANEL_RES_X / width, PANEL_RES_Y / height));
  offsetX = (PANEL_RES_X - width * scale) / 2;
  offsetY = (PANEL_RES_Y - height * scale) / 2;

  display->clearScreen();
  return true;
}

int gifPlayFrame(int &frameMs) {
  frameMs = 0;
  int more = gif.playFrame(false, &frameMs);
  if (more < 0) {
    Serial.printf("GIF decode failed: error %d\n", gif.getLastError());
    return more;
  }
  if (frameMs < MIN_FRAME_MS) {
    frameMs = DEFAULT_FRAME_MS;
  }
  return more;
}

void gifRestart() { gif.reset(); }

void gifClose() { gif.close(); }
