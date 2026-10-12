#pragma once

// The arithmetic behind the sound visualizers: a 1024-point FFT, grouping of
// its output into bands evenly spaced by pitch, and decibel scaling. No
// hardware access, so it can be tested with made-up signals.

namespace spectrum {

static const int FFT_SIZE = 1024;
static const int BANDS = 32;

// Builds the lookup tables. Call once before anything else.
void begin(float sampleRateHz, float lowHz, float highHz);

// Analyses FFT_SIZE samples. A Hann window is applied here.
void analyse(const float *samples);

// The strongest component in a band from the last analyse(), as a sine-wave
// amplitude in the units of the samples.
float bandMagnitude(int band);

// The frequency at the bottom edge of a band; band BANDS is the top of the range.
float bandEdgeHz(int band);

float toDb(float value);

// Maps a level in dB onto 0-1, where `topDb` is 1 and `topDb - rangeDb` is 0.
float scale(float db, float topDb, float rangeDb);

} // namespace spectrum
