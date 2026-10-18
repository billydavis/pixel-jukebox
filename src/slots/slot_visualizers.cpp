#include "slots/slot_visualizers.h"

#include "display/display.h"

static const int W = PANEL_RES_X;
static const int H = PANEL_RES_Y;

// About 3.3 KB, so shared rather than held in each slot, and in PSRAM: the
// microphones' own buffers already take internal RAM while sound mode is on.
static AudioFrame *frame = nullptr;

// Colour for a hue in degrees at full saturation.
static uint16_t hue(float degrees, float value = 1.0f) {
  float h = fmodf(degrees, 360.0f) / 60.0f;
  float x = 1.0f - fabsf(fmodf(h, 2.0f) - 1.0f);
  float r = 0, g = 0, b = 0;
  switch ((int)h) {
    case 0: r = 1; g = x; break;
    case 1: r = x; g = 1; break;
    case 2: g = 1; b = x; break;
    case 3: g = x; b = 1; break;
    case 4: r = x; b = 1; break;
    default: r = 1; b = x; break;
  }
  return display->color565(r * value * 255, g * value * 255, b * value * 255);
}

bool VisualizerSlot::start() {
  if (!frame) {
    frame = (AudioFrame *)ps_calloc(1, sizeof(AudioFrame));
  }
  if (!frame || !analysisBegin()) {
    return false;
  }
  display->clearScreen();
  reset();
  return true;
}

uint32_t VisualizerSlot::update() {
  if (analysisCapture(*frame)) {
    draw(*frame);
    return 0; // waiting for the next block of audio sets the pace
  }
  return 50;
}

bool VisualizerSlot::finished() const { return false; }

void VisualizerSlot::redraw() {
  display->clearScreen();
  reset();
}

// --- Spectrum bars ---------------------------------------------------------

static const int BAR_WIDTH = W / ANALYSIS_BANDS;
static const float BAR_FALL = 2.5f;  // pixels per frame
static const float CAP_FALL = 0.6f;
static const uint8_t CAP_HOLD_FRAMES = 10;

// Mint at the bottom through blue and violet to pink at the top.
static uint16_t barColour(int row) { return hue(165.0f + 170.0f * row / (H - 1)); }

void SpectrumSlot::reset() {
  for (int i = 0; i < ANALYSIS_BANDS; i++) {
    heights[i] = 0;
    caps[i] = 0;
    capHold[i] = 0;
    drawnHeight[i] = 0;
    drawnCap[i] = -1;
  }
}

void SpectrumSlot::draw(const AudioFrame &audio) {
  uint16_t white = display->color565(255, 255, 255);
  for (int i = 0; i < ANALYSIS_BANDS; i++) {
    float target = audio.bands[i] * (H - 1);
    heights[i] = target > heights[i] ? target : max(0.0f, heights[i] - BAR_FALL);
    if (heights[i] >= caps[i]) {
      caps[i] = heights[i];
      capHold[i] = CAP_HOLD_FRAMES;
    } else if (capHold[i] > 0) {
      capHold[i]--;
    } else {
      caps[i] = max(heights[i], caps[i] - CAP_FALL);
    }

    int height = (int)heights[i];
    int cap = (int)caps[i];
    int x = i * BAR_WIDTH;

    // Only the rows that changed are repainted. Row 0 is the bottom of the panel.
    for (int row = drawnHeight[i]; row < height; row++) {
      display->drawFastHLine(x, H - 1 - row, BAR_WIDTH, barColour(row));
    }
    for (int row = height; row < drawnHeight[i]; row++) {
      display->drawFastHLine(x, H - 1 - row, BAR_WIDTH, 0);
    }
    if (drawnCap[i] >= 0 && drawnCap[i] != cap) {
      uint16_t under = drawnCap[i] < height ? barColour(drawnCap[i]) : 0;
      display->drawFastHLine(x, H - 1 - drawnCap[i], BAR_WIDTH, under);
    }
    display->drawFastHLine(x, H - 1 - cap, BAR_WIDTH, white);
    drawnHeight[i] = height;
    drawnCap[i] = cap;
  }
}

// --- VU meter --------------------------------------------------------------

static const int SEGMENTS = 13;
static const int SEGMENT_WIDTH = 3;
static const int SEGMENT_PITCH = 4;
static const int METER_LEFT = 11;
static const int METER_HEIGHT = 14;
static const int METER_TOPS[2] = {10, 40};
static const float METER_FALL = 0.035f; // per frame
static const float PEAK_FALL = 0.02f;
static const uint8_t PEAK_HOLD_FRAMES = 20;

// Mint, then amber, then pink for the loudest segments.
static uint16_t segmentColour(int segment, bool lit) {
  float value = lit ? 1.0f : 0.2f;
  if (segment >= SEGMENTS - 2) {
    return hue(335, value);
  }
  if (segment >= SEGMENTS - 5) {
    return hue(40, value);
  }
  return hue(165, value);
}

static void paintSegment(int segment, int top, uint16_t colour) {
  display->fillRect(METER_LEFT + segment * SEGMENT_PITCH, top, SEGMENT_WIDTH, METER_HEIGHT, colour);
}

void VuSlot::reset() {
  display->setTextSize(1);
  display->setTextWrap(false);
  display->setTextColor(display->color565(150, 150, 150));
  const char *labels[2] = {"L", "R"};
  for (int i = 0; i < 2; i++) {
    meters[i] = {0, 0, 0, 0, -1};
    display->setCursor(2, METER_TOPS[i] + 3);
    display->print(labels[i]);
    for (int segment = 0; segment < SEGMENTS; segment++) {
      paintSegment(segment, METER_TOPS[i], segmentColour(segment, false));
    }
  }
}

void VuSlot::drawMeter(Meter &meter, float level, int top) {
  meter.level = level > meter.level ? level : max(0.0f, meter.level - METER_FALL);
  if (meter.level >= meter.peak) {
    meter.peak = meter.level;
    meter.peakHold = PEAK_HOLD_FRAMES;
  } else if (meter.peakHold > 0) {
    meter.peakHold--;
  } else {
    meter.peak = max(meter.level, meter.peak - PEAK_FALL);
  }

  int lit = (int)roundf(meter.level * SEGMENTS);
  int peak = min(SEGMENTS - 1, (int)(meter.peak * SEGMENTS));
  if (lit == meter.drawnLit && peak == meter.drawnPeak) {
    return;
  }
  for (int segment = 0; segment < SEGMENTS; segment++) {
    bool was = segment < meter.drawnLit || segment == meter.drawnPeak;
    bool now = segment < lit || segment == peak;
    if (was != now) {
      paintSegment(segment, top, segmentColour(segment, now));
    }
  }
  meter.drawnLit = lit;
  meter.drawnPeak = peak;
}

void VuSlot::draw(const AudioFrame &audio) {
  drawMeter(meters[0], audio.levelLeft, METER_TOPS[0]);
  drawMeter(meters[1], audio.levelRight, METER_TOPS[1]);
}

// --- Oscilloscope ----------------------------------------------------------

static const int SCOPE_STEP = 3; // samples per column: the panel spans 12 ms
static const int SCOPE_REACH = H / 2 - 3;

void ScopeSlot::reset() { hasTrace = false; }

void ScopeSlot::draw(const AudioFrame &audio) {
  // Start on a rising zero crossing so a steady tone stands still.
  int origin = 0;
  int searchEnd = ANALYSIS_SAMPLES - W * SCOPE_STEP - 1;
  for (int i = 1; i < searchEnd; i++) {
    if (audio.mono[i - 1] < 0 && audio.mono[i] >= 0) {
      origin = i;
      break;
    }
  }

  int8_t next[W];
  for (int x = 0; x < W; x++) {
    float sample = audio.mono[origin + x * SCOPE_STEP] * audio.waveScale;
    next[x] = (int8_t)constrain(H / 2 - (int)(sample * SCOPE_REACH), 0, H - 1);
  }

  // Rub out the old line and draw the new one, rather than clearing the panel.
  if (hasTrace) {
    for (int x = 1; x < W; x++) {
      display->drawLine(x - 1, trace[x - 1], x, trace[x], 0);
    }
  }
  display->drawFastHLine(0, H / 2, W, hue(240, 0.2f));
  uint16_t colour = hue(165);
  for (int x = 1; x < W; x++) {
    display->drawLine(x - 1, next[x - 1], x, next[x], colour);
  }
  memcpy(trace, next, sizeof(trace));
  hasTrace = true;
}

// --- Fire ------------------------------------------------------------------

static uint16_t firePalette[256];
static bool firePaletteReady = false;

// Black, deep red, orange, yellow, white.
static void buildFirePalette() {
  for (int i = 0; i < 256; i++) {
    int r = min(255, i * 3);
    int g = constrain((i - 85) * 3, 0, 255);
    int b = constrain((i - 170) * 3, 0, 255);
    firePalette[i] = display->color565(r, g, b);
  }
  firePaletteReady = true;
}

void FireSlot::reset() {
  if (!firePaletteReady) {
    buildFirePalette();
  }
  memset(heat, 0, sizeof(heat));
  beats.reset();
}

// One step of the fire: new fuel at the bottom, then each pixel takes the
// average of the pixels below it, losing a little heat; a smaller divisor
// loses less, so the flames reach higher.
void FireSlot::burn(int fuel, int divisor) {
  for (int row = H; row < H + 2; row++) {
    for (int x = 0; x < W; x++) {
      heat[row][x] = random(fuel / 3, fuel + 1);
    }
  }
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int left = heat[y + 1][x > 0 ? x - 1 : x];
      int right = heat[y + 1][x < W - 1 ? x + 1 : x];
      int sum = left + heat[y + 1][x] + right + heat[y + 2][x];
      heat[y][x] = min(255, sum * 32 / divisor);
    }
  }
}

void FireSlot::draw(const AudioFrame &audio) {
  // The level is scaled to the room, so steady music keeps it high: it sets
  // a steady fire. Beats are what make it move (beat_follower.h).
  float punch = beats.update(audio.bass);
  float level = audio.level;
  int fuel = 25 + (int)(level * 110) + (int)(punch * 120);
  int divisor = 140 - (int)(level * 4) - (int)(punch * 10);

  // Heat climbs about a row per step; two or three steps per audio frame let
  // a beat reach the top of the panel in well under a second instead of two.
  int steps = punch > 0.5f ? 3 : 2;
  for (int i = 0; i < steps; i++) {
    burn(fuel, divisor);
  }

  // A beat also flashes the whole fire brighter at once, before the new
  // flames have had time to climb.
  int boost = 256 + (int)(punch * 150);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      display->drawPixel(x, y, firePalette[min(255, heat[y][x] * boost >> 8)]);
    }
  }
}

// --- Shared by the newer visualizers ---------------------------------------

// Large working buffers live in PSRAM, made the first time they are needed
// and kept: internal RAM is what Wi-Fi and the TLS sessions need.
static uint8_t *psramBuffer(uint8_t *&buffer, size_t bytes) {
  if (!buffer) {
    buffer = (uint8_t *)ps_malloc(bytes);
  }
  if (buffer) {
    memset(buffer, 0, bytes);
  }
  return buffer;
}

// The pitch that is loudest right now, as a hue: red for bass through to
// violet for treble.
static float loudestHue(const AudioFrame &audio) {
  int best = 0;
  for (int band = 1; band < ANALYSIS_BANDS; band++) {
    if (audio.bands[band] > audio.bands[best]) {
      best = band;
    }
  }
  return 300.0f * best / (ANALYSIS_BANDS - 1);
}

// --- Pulse rings -------------------------------------------------------------

static const int RING_STEPS = 92;        // distance from the middle in half pixels
static const float RING_SPEED = 4.5f;    // half pixels per frame: edge to edge in about 0.65 s
static const float RING_FADE = 0.96f;
static const float RING_WIDTH = 3.5f;    // half pixels either side
static const float RING_TRIGGER = 0.18f; // punch needed for a new ring
static const int RING_GAP_FRAMES = 2;    // at most one ring per 64 ms
static const float RING_MIN_STRENGTH = 0.55f; // even a small beat makes a clear ring
static uint8_t *ringDistance = nullptr;  // per pixel, in half pixels

void RingsSlot::reset() {
  if (!ringDistance && psramBuffer(ringDistance, W * H)) {
    for (int y = 0; y < H; y++) {
      for (int x = 0; x < W; x++) {
        float dx = x - (W - 1) / 2.0f, dy = y - (H - 1) / 2.0f;
        ringDistance[y * W + x] = (uint8_t)min(RING_STEPS - 1, (int)(sqrtf(dx * dx + dy * dy) * 2));
      }
    }
  }
  for (Ring &ring : rings) {
    ring.strength = 0;
  }
  beats.reset();
  sinceRing = RING_GAP_FRAMES;
}

void RingsSlot::draw(const AudioFrame &audio) {
  if (!ringDistance) {
    return;
  }
  // Snares and voices make rings too, not only the bass.
  float punch = beats.update(max(audio.bass, audio.level));
  if (++sinceRing >= RING_GAP_FRAMES && punch >= RING_TRIGGER) {
    for (Ring &ring : rings) {
      if (ring.strength <= 0.02f) {
        ring = {0, loudestHue(audio), RING_MIN_STRENGTH + (1 - RING_MIN_STRENGTH) * punch};
        sinceRing = 0;
        break;
      }
    }
  }

  // Colour by distance from the middle, then paint each pixel from that.
  float red[RING_STEPS] = {}, green[RING_STEPS] = {}, blue[RING_STEPS] = {};
  // A glow in the middle that swells with the level and flashes on the beat.
  float glow = 4 + audio.level * 16 + punch * 14; // half pixels
  for (int d = 0; d < RING_STEPS && d < glow; d++) {
    float v = (0.2f + 0.5f * punch) * (1 - d / glow);
    red[d] += v * 0.3f;
    green[d] += v * 0.9f;
    blue[d] += v * 0.8f;
  }
  for (Ring &ring : rings) {
    if (ring.strength <= 0.02f) {
      continue;
    }
    uint16_t c = hue(ring.hue);
    float r = ((c >> 11) & 0x1f) / 31.0f, g = ((c >> 5) & 0x3f) / 63.0f, b = (c & 0x1f) / 31.0f;
    int from = max(0, (int)(ring.radius - RING_WIDTH));
    int to = min(RING_STEPS - 1, (int)(ring.radius + RING_WIDTH));
    for (int d = from; d <= to; d++) {
      float v = ring.strength * (1 - fabsf(d - ring.radius) / RING_WIDTH);
      if (v > 0) {
        red[d] += r * v;
        green[d] += g * v;
        blue[d] += b * v;
      }
    }
    ring.radius += RING_SPEED;
    ring.strength = ring.radius >= RING_STEPS ? 0 : ring.strength * RING_FADE;
  }
  uint16_t colour[RING_STEPS];
  for (int d = 0; d < RING_STEPS; d++) {
    colour[d] = display->color565(min(255, (int)(red[d] * 255)), min(255, (int)(green[d] * 255)),
                                  min(255, (int)(blue[d] * 255)));
  }
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      display->drawPixel(x, y, colour[ringDistance[y * W + x]]);
    }
  }
}

// --- Starfield ---------------------------------------------------------------

static uint8_t *starGlow = nullptr; // per pixel brightness, fading
static const float STAR_FOCAL = 22.0f;

static void glowAt(int x, int y, int add) {
  if (x >= 0 && x < W && y >= 0 && y < H) {
    uint8_t &p = starGlow[y * W + x];
    p = min(255, p + add);
  }
}

void StarfieldSlot::respawn(Star &star, bool anywhere) {
  star.x = random(-1000, 1001) / 1000.0f;
  star.y = random(-1000, 1001) / 1000.0f;
  star.z = anywhere ? random(50, 1001) / 1000.0f : 1.0f;
}

void StarfieldSlot::reset() {
  psramBuffer(starGlow, W * H);
  for (Star &star : stars) {
    respawn(star, true);
  }
  beats.reset();
}

void StarfieldSlot::draw(const AudioFrame &audio) {
  if (!starGlow) {
    return;
  }
  float punch = beats.update(audio.bass);
  float speed = 0.006f + audio.level * 0.02f + punch * 0.05f;
  // Trails fade fast normally and linger on the beat, which draws streaks.
  int keep = 140 + (int)(punch * 100);
  for (int i = 0; i < W * H; i++) {
    starGlow[i] = starGlow[i] * keep >> 8;
  }
  const float cx = (W - 1) / 2.0f, cy = (H - 1) / 2.0f;
  for (Star &star : stars) {
    float oldZ = star.z;
    star.z -= speed * (1.2f - star.z); // nearer stars move faster
    if (star.z <= 0.04f) {
      respawn(star, false);
      continue;
    }
    int x0 = (int)(cx + star.x * STAR_FOCAL / oldZ), y0 = (int)(cy + star.y * STAR_FOCAL / oldZ);
    int x1 = (int)(cx + star.x * STAR_FOCAL / star.z), y1 = (int)(cy + star.y * STAR_FOCAL / star.z);
    if (x1 < -4 || x1 > W + 4 || y1 < -4 || y1 > H + 4) {
      respawn(star, false);
      continue;
    }
    int bright = 60 + (int)((1 - star.z) * 195);
    int steps = max(abs(x1 - x0), abs(y1 - y0));
    int span = max(1, steps);
    for (int s = 0; s <= steps; s++) { // the line from where it was: a streak when fast
      glowAt(x0 + (x1 - x0) * s / span, y0 + (y1 - y0) * s / span, bright * (s + 1) / (steps + 1));
    }
  }
  // White stars, warming towards gold on the beat.
  int greenKeep = 255 - (int)(punch * 40), blueKeep = 255 - (int)(punch * 160);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int v = starGlow[y * W + x];
      display->drawPixel(x, y, display->color565(v, v * greenKeep >> 8, v * blueKeep >> 8));
    }
  }
}

// --- Stereo scope (goniometer) -------------------------------------------------

static uint8_t *stereoGlow = nullptr;
static const float STEREO_GAIN = 26.0f; // pixels for a full-scale sample

void StereoSlot::reset() {
  psramBuffer(stereoGlow, W * H);
  beats.reset();
}

void StereoSlot::draw(const AudioFrame &audio) {
  if (!stereoGlow) {
    return;
  }
  float punch = beats.update(audio.level);
  for (int i = 0; i < W * H; i++) {
    stereoGlow[i] = stereoGlow[i] * 180 >> 8;
  }
  // Mid (what both hear) upwards, side (the difference) across, turned 45
  // degrees from plain left-against-right as studio meters do.
  for (int i = 0; i < ANALYSIS_SAMPLES; i += 2) {
    float l = audio.left[i] * audio.waveScale, r = audio.right[i] * audio.waveScale;
    int x = (int)((W - 1) / 2.0f + (l - r) * STEREO_GAIN);
    int y = (int)((H - 1) / 2.0f - (l + r) * STEREO_GAIN * 0.5f);
    if (x >= 0 && x < W && y >= 0 && y < H) {
      uint8_t &p = stereoGlow[y * W + x];
      p = min(255, p + 70);
    }
  }
  // Mint, flashing pink on the beat; the brightest points go white.
  int pink = (int)(punch * 255);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int v = stereoGlow[y * W + x];
      int hot = max(0, v - 160) * 2;
      int r = min(255, (v * (60 + pink) >> 8) + hot);
      int g = min(255, (v * (230 - pink / 2) >> 8) + hot);
      int b = min(255, (v * (180 + pink / 4) >> 8) + hot);
      display->drawPixel(x, y, display->color565(r, g, b));
    }
  }
}

// --- Hi-fi meter ---------------------------------------------------------------

static const int HIFI_SEGMENTS = 21;     // two pixels tall, one apart
static const int HIFI_COLUMN_PITCH = W / 16;
static const float HIFI_FALL = 0.9f;     // segments per frame
static const float HIFI_PEAK_FALL = 0.35f;
static const uint8_t HIFI_PEAK_HOLD = 18;

static uint16_t hifiColour(int segment, bool lit) {
  int r, g, b;
  if (segment >= HIFI_SEGMENTS - 3) {
    r = 255, g = 40, b = 30; // red
  } else if (segment >= HIFI_SEGMENTS - 8) {
    r = 255, g = 170, b = 0; // amber
  } else {
    r = 40, g = 230, b = 80; // green
  }
  return lit ? display->color565(r, g, b) : display->color565(r / 14, g / 14, b / 14);
}

static void paintHifi(int column, int segment, uint16_t colour) {
  int y = H - 2 - segment * 3;
  display->fillRect(column * HIFI_COLUMN_PITCH, y, HIFI_COLUMN_PITCH - 1, 2, colour);
}

void HifiSlot::reset() {
  for (int c = 0; c < COLUMNS; c++) {
    level[c] = peak[c] = 0;
    peakHold[c] = 0;
    drawnLit[c] = 0;
    drawnPeak[c] = -1;
    for (int s = 0; s < HIFI_SEGMENTS; s++) {
      paintHifi(c, s, hifiColour(s, false)); // unlit LEDs show faintly, as on the real thing
    }
  }
}

void HifiSlot::draw(const AudioFrame &audio) {
  for (int c = 0; c < COLUMNS; c++) {
    float v = max(audio.bands[c * 2], audio.bands[c * 2 + 1]) * HIFI_SEGMENTS;
    level[c] = v > level[c] ? v : max(v, level[c] - HIFI_FALL);
    if (level[c] >= peak[c]) {
      peak[c] = level[c];
      peakHold[c] = HIFI_PEAK_HOLD;
    } else if (peakHold[c] > 0) {
      peakHold[c]--;
    } else {
      peak[c] = max(0.0f, peak[c] - HIFI_PEAK_FALL);
    }

    int lit = min(HIFI_SEGMENTS, (int)level[c]);
    int peakAt = min(HIFI_SEGMENTS - 1, (int)peak[c]);
    if (peakAt < lit || peak[c] < 1) {
      peakAt = -1; // inside the bar, or nothing to mark
    }
    // Only the segments that changed are painted.
    for (int s = min(lit, (int)drawnLit[c]); s < max(lit, (int)drawnLit[c]); s++) {
      paintHifi(c, s, hifiColour(s, s < lit));
    }
    if (peakAt != drawnPeak[c]) {
      if (drawnPeak[c] >= lit) {
        paintHifi(c, drawnPeak[c], hifiColour(drawnPeak[c], false));
      }
      if (peakAt >= 0) {
        paintHifi(c, peakAt, hifiColour(peakAt, true));
      }
    }
    drawnLit[c] = lit;
    drawnPeak[c] = peakAt;
  }
}
