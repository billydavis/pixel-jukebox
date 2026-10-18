#include "slots/slot_visualizers.h"

#include "display/display.h"

static const int W = PANEL_RES_X;
static const int H = PANEL_RES_Y;

static AudioFrame frame; // large, so shared rather than held in each slot

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
  if (!analysisBegin()) {
    return false;
  }
  display->clearScreen();
  reset();
  return true;
}

uint32_t VisualizerSlot::update() {
  if (analysisCapture(frame)) {
    draw(frame);
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
