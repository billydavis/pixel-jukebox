#include "audio/audio_analysis.h"

#include "audio/audio.h"
#include "spectrum_math.h"

static const float SAMPLE_RATE = 16000.0f;
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

// The FFT spans two blocks, so each frame overlaps the last by half.
static float history[spectrum::FFT_SIZE];
static bool tablesReady = false;

static float bandTopDb = BAND_MIN_TOP_DB;
static float levelTopDb = LEVEL_MIN_TOP_DB;
static float wavePeak = WAVE_MIN_PEAK;

static float rmsDb(const int16_t *samples) {
  float sum = 0;
  for (int i = 0; i < ANALYSIS_SAMPLES; i++) {
    sum += (float)samples[i] * samples[i];
  }
  return spectrum::toDb(sqrtf(sum / ANALYSIS_SAMPLES));
}

bool analysisBegin() {
  if (!audioBegin()) {
    return false;
  }
  if (!tablesReady) {
    spectrum::begin(SAMPLE_RATE, LOW_HZ, HIGH_HZ);
    tablesReady = true;
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
  spectrum::analyse(history);

  float bandDb[ANALYSIS_BANDS];
  float loudest = -100;
  for (int band = 0; band < ANALYSIS_BANDS; band++) {
    float octaves = log2f(spectrum::bandEdgeHz(band) / LOW_HZ);
    bandDb[band] = spectrum::toDb(spectrum::bandMagnitude(band)) + TILT_DB_PER_OCTAVE * octaves;
    loudest = max(loudest, bandDb[band]);
  }
  bandTopDb = max(max(loudest, bandTopDb - FALL_DB_PER_FRAME), BAND_MIN_TOP_DB);
  float bass = 0;
  for (int band = 0; band < ANALYSIS_BANDS; band++) {
    frame.bands[band] = spectrum::scale(bandDb[band], bandTopDb, RANGE_DB);
    if (band < 6) {
      bass += frame.bands[band] / 6;
    }
  }
  frame.bass = bass;

  float leftDb = rmsDb(frame.left);
  float rightDb = rmsDb(frame.right);
  levelTopDb = max(max(max(leftDb, rightDb), levelTopDb - FALL_DB_PER_FRAME), LEVEL_MIN_TOP_DB);
  frame.levelLeft = spectrum::scale(leftDb, levelTopDb, RANGE_DB);
  frame.levelRight = spectrum::scale(rightDb, levelTopDb, RANGE_DB);
  frame.level = max(frame.levelLeft, frame.levelRight);

  wavePeak = max(max((float)peak, wavePeak * 0.995f), WAVE_MIN_PEAK);
  frame.waveScale = 1.0f / wavePeak;
  return true;
}
