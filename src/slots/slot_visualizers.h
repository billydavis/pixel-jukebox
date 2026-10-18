#pragma once

#include "audio/audio_analysis.h"
#include "beat_follower.h"
#include "pins.h"
#include "slots/slot.h"

// Slots that draw from the microphones, used by sound mode (mode.cpp). One
// stays on screen until another is chosen, redrawing about 30 times a second,
// paced by the audio arriving.
class VisualizerSlot : public Slot {
 public:
  bool start() override;
  uint32_t update() override;
  bool finished() const override;
  void redraw() override;

 protected:
  // Clears any state and paints the parts that do not change.
  virtual void reset() = 0;
  // Paints one frame. Must not clear the panel, which would make it flicker.
  virtual void draw(const AudioFrame &frame) = 0;
};

// Thirty-two bars from bass to treble, with caps that hang at each peak.
class SpectrumSlot : public VisualizerSlot {
 public:
  const char *name() const override { return "spectrum"; }

 protected:
  void reset() override;
  void draw(const AudioFrame &frame) override;

 private:
  float heights[ANALYSIS_BANDS];
  float caps[ANALYSIS_BANDS];
  uint8_t capHold[ANALYSIS_BANDS];
  int8_t drawnHeight[ANALYSIS_BANDS];
  int8_t drawnCap[ANALYSIS_BANDS];
};

// Two segmented level meters, one per microphone, with peak markers.
class VuSlot : public VisualizerSlot {
 public:
  const char *name() const override { return "vu"; }

 protected:
  void reset() override;
  void draw(const AudioFrame &frame) override;

 private:
  struct Meter {
    float level;
    float peak;
    uint8_t peakHold;
    int8_t drawnLit;
    int8_t drawnPeak;
  };
  void drawMeter(Meter &meter, float level, int top);

  Meter meters[2];
};

// The sound wave itself, as a line across the panel.
class ScopeSlot : public VisualizerSlot {
 public:
  const char *name() const override { return "scope"; }

 protected:
  void reset() override;
  void draw(const AudioFrame &frame) override;

 private:
  int8_t trace[PANEL_RES_X];
  bool hasTrace;
};

// Flames that leap on the beat: kick drums throw fuel on the fire and flash it
// brighter, and the overall loudness keeps it burning between beats.
class FireSlot : public VisualizerSlot {
 public:
  const char *name() const override { return "fire"; }

 protected:
  void reset() override;
  void draw(const AudioFrame &frame) override;

 private:
  void burn(int fuel, int divisor);

  uint8_t heat[PANEL_RES_Y + 2][PANEL_RES_X]; // two hidden rows of fuel at the bottom
  BeatFollower beats;
};
