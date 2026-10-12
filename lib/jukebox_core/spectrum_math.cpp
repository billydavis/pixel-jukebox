#include "spectrum_math.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace spectrum {

static const float TWO_PI_F = 6.28318530718f;

static float window[FFT_SIZE];
static float twiddleCos[FFT_SIZE / 2];
static float twiddleSin[FFT_SIZE / 2];
static float re[FFT_SIZE];
static float im[FFT_SIZE];
static float bandStart[BANDS + 1]; // fractional FFT bin of each band edge
static float binHz = 1;

void begin(float sampleRateHz, float lowHz, float highHz) {
  binHz = sampleRateHz / FFT_SIZE;
  for (int i = 0; i < FFT_SIZE; i++) {
    window[i] = 0.5f - 0.5f * cosf(TWO_PI_F * i / (FFT_SIZE - 1)); // Hann
  }
  for (int i = 0; i < FFT_SIZE / 2; i++) {
    twiddleCos[i] = cosf(TWO_PI_F * i / FFT_SIZE);
    twiddleSin[i] = -sinf(TWO_PI_F * i / FFT_SIZE);
  }
  for (int i = 0; i <= BANDS; i++) {
    float hz = lowHz * powf(highHz / lowHz, (float)i / BANDS);
    bandStart[i] = hz / binHz;
  }
}

// In-place radix-2 FFT.
static void fft() {
  for (int i = 1, j = 0; i < FFT_SIZE; i++) {
    int bit = FFT_SIZE >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(re[i], re[j]);
      std::swap(im[i], im[j]);
    }
  }
  for (int size = 2; size <= FFT_SIZE; size <<= 1) {
    int half = size / 2;
    int step = FFT_SIZE / size;
    for (int start = 0; start < FFT_SIZE; start += size) {
      for (int k = 0; k < half; k++) {
        float c = twiddleCos[k * step];
        float s = twiddleSin[k * step];
        int a = start + k;
        int b = a + half;
        float tr = re[b] * c - im[b] * s;
        float ti = re[b] * s + im[b] * c;
        re[b] = re[a] - tr;
        im[b] = im[a] - ti;
        re[a] += tr;
        im[a] += ti;
      }
    }
  }
}

void analyse(const float *samples) {
  for (int i = 0; i < FFT_SIZE; i++) {
    re[i] = samples[i] * window[i];
    im[i] = 0;
  }
  fft();
}

static float magnitude(int bin) {
  return sqrtf(re[bin] * re[bin] + im[bin] * im[bin]) / (FFT_SIZE / 4); // Hann halves the level
}

// The strongest bin in a band; narrow bass bands interpolate between two bins.
float bandMagnitude(int band) {
  float from = bandStart[band];
  float to = bandStart[band + 1];
  int first = (int)ceilf(from);
  int last = (int)floorf(to);
  if (last <= first) {
    float centre = (from + to) / 2;
    int bin = (int)centre;
    float mix = centre - bin;
    return magnitude(bin) * (1 - mix) + magnitude(bin + 1) * mix;
  }
  float best = 0;
  for (int bin = first; bin <= last; bin++) {
    best = std::max(best, magnitude(bin));
  }
  return best;
}

float bandEdgeHz(int band) { return bandStart[band] * binHz; }

float toDb(float value) { return 20.0f * log10f(value + 0.001f); }

float scale(float db, float topDb, float rangeDb) {
  float value = (db - (topDb - rangeDb)) / rangeDb;
  return std::min(1.0f, std::max(0.0f, value));
}

} // namespace spectrum
