#include "audio_analysis.h"

#include "audio.h"

static const int FFT_SIZE = 1024; // two blocks, so each frame overlaps the last by half
static const float SAMPLE_RATE = 16000.0f;
static const float BIN_HZ = SAMPLE_RATE / FFT_SIZE;
static const float LOW_HZ = 60.0f;
static const float HIGH_HZ = 7000.0f;
static const float TILT_DB_PER_OCTAVE = 3.0f; // music is bass-heavy; this evens out the bars

// Automatic scaling, in decibels. The top of the scale follows the loudest
// recent sound and drifts back down slowly; it never drops so low that the
// room's background noise fills the display: measured in a quiet room, the
// loudest band sits near 38 dB and the overall level near 34 dB.
static const float RANGE_DB = 24.0f;
static const float FALL_DB_PER_FRAME = 0.12f;
static const float BAND_MIN_TOP_DB = 62.0f;
static const float LEVEL_MIN_TOP_DB = 58.0f;
static const float WAVE_MIN_PEAK = 600.0f;

static float window[FFT_SIZE];
static float twiddleCos[FFT_SIZE / 2];
static float twiddleSin[FFT_SIZE / 2];
static float history[FFT_SIZE]; // the last two blocks of mono audio
static float re[FFT_SIZE];
static float im[FFT_SIZE];
static float bandStart[ANALYSIS_BANDS + 1]; // fractional FFT bin of each band edge
static bool tablesReady = false;

static float bandTopDb = BAND_MIN_TOP_DB;
static float levelTopDb = LEVEL_MIN_TOP_DB;
static float wavePeak = WAVE_MIN_PEAK;

static void buildTables() {
  for (int i = 0; i < FFT_SIZE; i++) {
    window[i] = 0.5f - 0.5f * cosf(2.0f * PI * i / (FFT_SIZE - 1)); // Hann
  }
  for (int i = 0; i < FFT_SIZE / 2; i++) {
    twiddleCos[i] = cosf(2.0f * PI * i / FFT_SIZE);
    twiddleSin[i] = -sinf(2.0f * PI * i / FFT_SIZE);
  }
  for (int i = 0; i <= ANALYSIS_BANDS; i++) {
    float hz = LOW_HZ * powf(HIGH_HZ / LOW_HZ, (float)i / ANALYSIS_BANDS);
    bandStart[i] = hz / BIN_HZ;
  }
  tablesReady = true;
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

static float magnitude(int bin) {
  return sqrtf(re[bin] * re[bin] + im[bin] * im[bin]) / (FFT_SIZE / 4); // Hann halves the level
}

// The strongest bin in a band; narrow bass bands interpolate between two bins.
static float bandMagnitude(int band) {
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
    best = max(best, magnitude(bin));
  }
  return best;
}

static float toDb(float value) { return 20.0f * log10f(value + 0.001f); }

// Maps a level in dB onto 0-1 against a top that follows the loudest recent sound.
static float scale(float db, float topDb) {
  return constrain((db - (topDb - RANGE_DB)) / RANGE_DB, 0.0f, 1.0f);
}

static float rmsDb(const int16_t *samples) {
  float sum = 0;
  for (int i = 0; i < ANALYSIS_SAMPLES; i++) {
    sum += (float)samples[i] * samples[i];
  }
  return toDb(sqrtf(sum / ANALYSIS_SAMPLES));
}

bool analysisBegin() {
  if (!audioBegin()) {
    return false;
  }
  if (!tablesReady) {
    buildTables();
  }
  audioFlushInput();
  memset(history, 0, sizeof(history));
  return true;
}

bool analysisCapture(AudioFrame &frame) {
  if (!audioRead(frame.left, frame.right, ANALYSIS_SAMPLES)) {
    return false;
  }

  memmove(history, history + ANALYSIS_SAMPLES, ANALYSIS_SAMPLES * sizeof(float));
  int peak = 0;
  for (int i = 0; i < ANALYSIS_SAMPLES; i++) {
    int mixed = (frame.left[i] + frame.right[i]) / 2;
    frame.mono[i] = mixed;
    history[ANALYSIS_SAMPLES + i] = mixed;
    peak = max(peak, abs(mixed));
  }

  for (int i = 0; i < FFT_SIZE; i++) {
    re[i] = history[i] * window[i];
    im[i] = 0;
  }
  fft();

  float bandDb[ANALYSIS_BANDS];
  float loudest = -100;
  for (int band = 0; band < ANALYSIS_BANDS; band++) {
    float octaves = log2f(bandStart[band] * BIN_HZ / LOW_HZ);
    bandDb[band] = toDb(bandMagnitude(band)) + TILT_DB_PER_OCTAVE * octaves;
    loudest = max(loudest, bandDb[band]);
  }
  bandTopDb = max(max(loudest, bandTopDb - FALL_DB_PER_FRAME), BAND_MIN_TOP_DB);
  float bass = 0;
  for (int band = 0; band < ANALYSIS_BANDS; band++) {
    frame.bands[band] = scale(bandDb[band], bandTopDb);
    if (band < 6) {
      bass += frame.bands[band] / 6;
    }
  }
  frame.bass = bass;

  float leftDb = rmsDb(frame.left);
  float rightDb = rmsDb(frame.right);
  levelTopDb = max(max(max(leftDb, rightDb), levelTopDb - FALL_DB_PER_FRAME), LEVEL_MIN_TOP_DB);
  frame.levelLeft = scale(leftDb, levelTopDb);
  frame.levelRight = scale(rightDb, levelTopDb);
  frame.level = max(frame.levelLeft, frame.levelRight);

  wavePeak = max(max((float)peak, wavePeak * 0.995f), WAVE_MIN_PEAK);
  frame.waveScale = 1.0f / wavePeak;
  return true;
}
