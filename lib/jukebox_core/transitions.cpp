#include "transitions.h"

#include <math.h>
#include <string.h>

namespace transitions {

static const int N = SIZE;
static const uint16_t WHITE = 0xFFFF;

typedef void (*Renderer)(float t, uint32_t seed, const uint16_t *a, const uint16_t *b, uint16_t *out);

// --- helpers ---------------------------------------------------------------

static inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static inline float ease(float t) { return t * t * (3 - 2 * t); }

// 0 to SIZE inclusive, reaching SIZE exactly at v = 1.
static inline int steps(float v) { return (int)(clamp01(v) * N + 0.5f); }

static uint32_t hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// Blends two RGB565 colours; amount 0 gives `a`, 256 gives `b`.
static uint16_t mix(uint16_t a, uint16_t b, int amount) {
  int keep = 256 - amount;
  int r = (((a >> 11) & 0x1F) * keep + ((b >> 11) & 0x1F) * amount) >> 8;
  int g = (((a >> 5) & 0x3F) * keep + ((b >> 5) & 0x3F) * amount) >> 8;
  int bl = ((a & 0x1F) * keep + (b & 0x1F) * amount) >> 8;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// --- the transitions -------------------------------------------------------

static void crossfade(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  int amount = (int)(t * 256);
  for (int i = 0; i < PIXELS; i++) {
    out[i] = mix(a[i], b[i], amount);
  }
}

static void fadeThroughBlack(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  bool second = t >= 0.5f;
  int amount = (int)((second ? 2 * t - 1 : 1 - 2 * t) * 256);
  const uint16_t *source = second ? b : a;
  for (int i = 0; i < PIXELS; i++) {
    out[i] = mix(0, source[i], amount);
  }
}

// The new image pushes the old one off, from one of four sides.
static void slide(float t, uint32_t seed, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  int shift = steps(ease(t));
  int direction = seed & 3;
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      int along = direction < 2 ? x : y;
      int moved = (direction & 1) ? along - shift : along + shift;
      const uint16_t *source = a;
      if (moved >= N) {
        moved -= N;
        source = b;
      } else if (moved < 0) {
        moved += N;
        source = b;
      }
      out[y * N + x] = direction < 2 ? source[y * N + moved] : source[moved * N + x];
    }
  }
}

// A straight edge sweeps across, from one of four sides.
static void wipe(float t, uint32_t seed, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  int edge = steps(t);
  int direction = seed & 3;
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      int along = direction < 2 ? x : y;
      bool revealed = (direction & 1) ? along >= N - edge : along < edge;
      out[y * N + x] = revealed ? b[y * N + x] : a[y * N + x];
    }
  }
}

// A circle opens from the centre.
static void iris(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  float radius = t * 45.5f; // just past the corners
  float limit = radius * radius;
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      float dx = x - 31.5f;
      float dy = y - 31.5f;
      out[y * N + x] = dx * dx + dy * dy < limit ? b[y * N + x] : a[y * N + x];
    }
  }
}

// Eight horizontal strips open together.
static void blinds(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  int rows = (int)(t * 8 + 0.5f);
  for (int y = 0; y < N; y++) {
    const uint16_t *source = (y % 8) < rows ? b : a;
    memcpy(out + y * N, source + y * N, N * sizeof(uint16_t));
  }
}

// The image breaks into bigger blocks, swaps at the coarsest, then sharpens.
static void mosaic(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  bool second = t >= 0.5f;
  float coarseness = second ? (1 - t) * 2 : t * 2; // 0 -> 1 -> 0
  int block = 1 << (int)(coarseness * 4.999f);     // 1, 2, 4, 8, 16
  const uint16_t *source = second ? b : a;
  for (int y = 0; y < N; y++) {
    int sampleY = y / block * block + block / 2;
    for (int x = 0; x < N; x++) {
      int sampleX = x / block * block + block / 2;
      out[y * N + x] = source[sampleY * N + sampleX];
    }
  }
}

// Pixels switch over one at a time in random order.
static void dissolve(float t, uint32_t seed, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  for (int i = 0; i < PIXELS; i++) {
    float threshold = (hash((uint32_t)i + seed * 4099U) & 0xFFFF) / 65536.0f;
    out[i] = t > threshold ? b[i] : a[i];
  }
}

// Columns of the old image slide down at different times, uncovering the new one.
static void melt(float t, uint32_t seed, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  int wander = hash(seed) % 16;
  for (int x = 0; x < N; x++) {
    wander += (int)(hash(seed + x * 31U) % 5) - 2; // neighbouring columns stay close
    wander = wander < 0 ? 0 : wander > 15 ? 15 : wander;
    float delay = wander / 15.0f * 0.4f;
    int shift = steps((t - delay) / 0.6f);
    for (int y = 0; y < N; y++) {
      out[y * N + x] = y >= shift ? a[(y - shift) * N + x] : b[y * N + x];
    }
  }
}

// Alternate squares open from their centres, then the rest.
static void checkerboard(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      bool first = (((x / 8) + (y / 8)) & 1) == 0;
      float opened = clamp01(first ? t * 2 : t * 2 - 1) * 4; // half-width of the opening
      float dx = fabsf((x % 8) - 3.5f);
      float dy = fabsf((y % 8) - 3.5f);
      out[y * N + x] = (dx > dy ? dx : dy) < opened ? b[y * N + x] : a[y * N + x];
    }
  }
}

// The old image squashes to a bright line, like a TV switching off, and the
// new one opens out from it.
static void scanline(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  bool second = t >= 0.5f;
  float half = (second ? 2 * t - 1 : 1 - 2 * t) * (N / 2); // half the height still showing
  const uint16_t *source = second ? b : a;
  for (int y = 0; y < N; y++) {
    float fromCentre = fabsf(y - 31.5f);
    uint16_t *row = out + y * N;
    if (half < 1 && fromCentre < 1) {
      for (int x = 0; x < N; x++) {
        row[x] = WHITE;
      }
    } else if (fromCentre < half) {
      int sampleY = (int)((y - (N / 2 - half)) / (2 * half) * N);
      sampleY = sampleY < 0 ? 0 : sampleY > N - 1 ? N - 1 : sampleY;
      memcpy(row, source + sampleY * N, N * sizeof(uint16_t));
    } else {
      memset(row, 0, N * sizeof(uint16_t));
    }
  }
}

// Squares switch over in a spiral from the top-left corner to the centre.
static void spiral(float t, uint32_t, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  static const int RING_START[4] = {0, 28, 48, 60}; // cells before each ring of the 8x8 grid
  int reached = (int)(t * 64 + 0.5f);
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      int cx = x / 8;
      int cy = y / 8;
      int ring = cx;
      ring = cy < ring ? cy : ring;
      ring = 7 - cx < ring ? 7 - cx : ring;
      ring = 7 - cy < ring ? 7 - cy : ring;
      int side = 7 - 2 * ring; // steps along one edge of this ring
      int lx = cx - ring;
      int ly = cy - ring;
      int position;
      if (ly == 0) {
        position = lx;
      } else if (lx == side) {
        position = side + ly;
      } else if (ly == side) {
        position = 2 * side + (side - lx);
      } else {
        position = 3 * side + (side - ly);
      }
      out[y * N + x] = reached > RING_START[ring] + position ? b[y * N + x] : a[y * N + x];
    }
  }
}

// The new image drops into place a column at a time.
static void rain(float t, uint32_t seed, const uint16_t *a, const uint16_t *b, uint16_t *out) {
  for (int x = 0; x < N; x++) {
    float delay = (hash(seed + x * 7919U) & 0xFF) / 255.0f * 0.5f;
    int rows = steps((t - delay) / 0.5f);
    for (int y = 0; y < N; y++) {
      out[y * N + x] = y < rows ? b[(y + N - rows) * N + x] : a[y * N + x];
    }
  }
}

// --- the list --------------------------------------------------------------

struct Entry {
  Info info;
  Renderer render;
};

static const Entry ENTRIES[] = {
    {{"crossfade", "Crossfade"}, crossfade},
    {{"fade", "Fade through black"}, fadeThroughBlack},
    {{"slide", "Slide"}, slide},
    {{"wipe", "Wipe"}, wipe},
    {{"iris", "Iris"}, iris},
    {{"blinds", "Blinds"}, blinds},
    {{"mosaic", "Mosaic"}, mosaic},
    {{"dissolve", "Dissolve"}, dissolve},
    {{"melt", "Melt"}, melt},
    {{"checkerboard", "Checkerboard"}, checkerboard},
    {{"scanline", "Scanline"}, scanline},
    {{"spiral", "Spiral"}, spiral},
    {{"rain", "Rain"}, rain},
};
static const int COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);

int count() { return COUNT; }

const Info &info(int index) { return ENTRIES[index].info; }

int find(const char *name) {
  for (int i = 0; i < COUNT; i++) {
    if (strcmp(ENTRIES[i].info.name, name) == 0) {
      return i;
    }
  }
  return -1;
}

void render(int index, float progress, uint32_t seed, const uint16_t *from, const uint16_t *to,
            uint16_t *out) {
  ENTRIES[index].render(clamp01(progress), seed, from, to, out);
}

} // namespace transitions
