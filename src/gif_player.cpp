#include "gif_player.h"

#include <AnimatedGIF.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"
#include "display.h"
#include "pins.h"

static const int MAX_GIF_BYTES = 4 * 1024 * 1024;
static const uint32_t DOWNLOAD_STALL_MS = 10000;
static const uint32_t MAX_SHOW_MS = 5 * 60 * 1000;
static const int MIN_FRAME_MS = 20;
static const int DEFAULT_FRAME_MS = 100; // what browsers use for near-zero delays

static AnimatedGIF gif; // large (LZW buffers), so kept off the stack
static int scale = 1;
static int offsetX = 0;
static int offsetY = 0;

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

  uint8_t *buffer = (uint8_t *)ps_malloc(length);
  if (!buffer) {
    buffer = (uint8_t *)malloc(length);
  }
  if (!buffer) {
    http.end();
    return nullptr;
  }

  WiFiClient *stream = http.getStreamPtr();
  int received = 0;
  uint32_t lastData = millis();
  while (received < length && millis() - lastData < DOWNLOAD_STALL_MS) {
    int count = stream->readBytes(buffer + received, min(length - received, 4096));
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

bool playGif(uint8_t *data, size_t size, uint32_t dwellMs) {
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

  uint32_t start = millis();
  int framesThisLoop = 0;
  bool drewAnything = false;
  while (millis() - start < MAX_SHOW_MS) {
    uint32_t frameStart = millis();
    int frameMs = 0;
    int more = gif.playFrame(false, &frameMs);
    if (more < 0) {
      Serial.printf("GIF decode failed: error %d\n", gif.getLastError());
      break;
    }
    drewAnything = true;
    framesThisLoop++;

    if (more == 0 && framesThisLoop == 1) {
      delay(dwellMs); // still image
      break;
    }

    if (frameMs < MIN_FRAME_MS) {
      frameMs = DEFAULT_FRAME_MS;
    }
    uint32_t spent = millis() - frameStart;
    if (spent < (uint32_t)frameMs) {
      delay(frameMs - spent);
    }

    if (more == 0) {
      if (millis() - start >= dwellMs) {
        break;
      }
      gif.reset();
      framesThisLoop = 0;
    }
  }

  gif.close();
  return drewAnything;
}
