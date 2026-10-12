#pragma once

#include <Arduino.h>

// Turns microphone input into the numbers the visualizer slots draw from.
// Levels are scaled automatically to the loudness of the room, so quiet music
// and loud music both fill the panel.

static const int ANALYSIS_SAMPLES = 512; // per frame: 32 ms at 16 kHz
static const int ANALYSIS_BANDS = 32;    // 60 Hz to 7 kHz, evenly spaced by pitch

struct AudioFrame {
  int16_t left[ANALYSIS_SAMPLES]; // the newest block of audio
  int16_t right[ANALYSIS_SAMPLES];
  int16_t mono[ANALYSIS_SAMPLES];

  float bands[ANALYSIS_BANDS]; // 0-1, bass first
  float levelLeft;             // 0-1 loudness per microphone
  float levelRight;
  float level;      // 0-1 overall loudness
  float bass;       // 0-1, the lowest bands
  float waveScale;  // multiply a sample by this to get roughly -1 to 1
};

// Starts audio if needed and discards stale input. False if audio is unavailable.
bool analysisBegin();

// Waits for the next block of audio (about 32 ms) and analyses it.
bool analysisCapture(AudioFrame &frame);
