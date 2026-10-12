#include "diagnostics.h"

#include <Arduino.h>

#include "audio.h"
#include "display.h"
#include "pins.h"

#ifdef SPEAKER_TEST
// Lists the I2C devices, then plays a rising chime and a steady tone three times.
static void runSpeakerTest() {
  String devices = audioScanI2c();
  Serial.printf("I2C devices: %s\n", devices.c_str());
  showMessage("I2C", devices, colour::amber());
  delay(4000);

  if (!audioBegin()) {
    Serial.println("Audio init failed");
    showMessage("Speaker", "init failed", colour::red());
    delay(5000);
    return;
  }
  Serial.println("Audio init OK, playing tones");
  showMessage("Speaker", "playing", colour::green());
  const float chime[] = {523.25f, 659.25f, 783.99f, 1046.5f}; // C5 E5 G5 C6
  for (int repeat = 0; repeat < 3; repeat++) {
    for (float note : chime) {
      audioTone(note, 250);
    }
    delay(300);
    audioTone(1000, 1000);
    delay(700);
  }
}
#endif

#ifdef MIC_TEST
// Shows the level of each microphone as a bar for a minute, and logs it.
static void runMicTest() {
  if (!audioBegin()) {
    Serial.println("Audio init failed");
    showMessage("Mic", "init failed", colour::red());
    delay(5000);
    return;
  }
  Serial.println("Audio init OK, measuring microphones");

  // Self-check: the microphones should hear the board's own speaker. The
  // reading after the tone comes from audio buffered while it was playing.
  audioMeasure(500); // discard the power-on transient
  MicLevels quiet = audioMeasure(300);
  audioTone(1000, 400);
  MicLevels tone = audioMeasure(100);
  bool heard = tone.rmsLeft > quiet.rmsLeft * 4 && tone.rmsRight > quiet.rmsRight * 4;
  Serial.printf("Self-check: quiet L %d R %d, 1 kHz tone L %d R %d: %s\n", quiet.rmsLeft,
                quiet.rmsRight, tone.rmsLeft, tone.rmsRight, heard ? "PASS" : "FAIL");
  showMessage("Mic", heard ? "self-check\npassed" : "self-check\nfailed",
              heard ? colour::green() : colour::red());
  delay(2000);

  const uint32_t testMs = 60000;
  const int barMax = PANEL_RES_X - 4;
  uint32_t start = millis();
  while (millis() - start < testMs) {
    MicLevels levels = audioMeasure(100);
    Serial.printf("mic rms L %5d R %5d  peak L %5d R %5d  frames %d\n", levels.rmsLeft,
                  levels.rmsRight, levels.peakLeft, levels.peakRight, levels.frames);

    // A square-root scale keeps quiet sounds visible.
    int left = min(barMax, (int)(sqrtf(levels.rmsLeft / 8000.0f) * barMax));
    int right = min(barMax, (int)(sqrtf(levels.rmsRight / 8000.0f) * barMax));
    display->clearScreen();
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(colour::amber());
    display->setCursor(2, 2);
    display->print("Mic test");
    display->setTextColor(display->color565(150, 150, 150));
    display->setCursor(2, 16);
    display->print("L");
    display->setCursor(2, 38);
    display->print("R");
    display->fillRect(2, 26, left, 8, colour::green());
    display->fillRect(2, 48, right, 8, colour::green());
  }
}
#endif

void runDiagnostics() {
#ifdef COLOUR_CHECK
  runColourCheck();
#endif
#ifdef SPEAKER_TEST
  runSpeakerTest();
#endif
#ifdef MIC_TEST
  runMicTest();
#endif
}
