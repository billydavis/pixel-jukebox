#include "app/diagnostics.h"

#include <Arduino.h>

#include "audio/audio.h"
#include "display/display.h"
#ifdef MOTION_TEST
#include "input/motion.h"
#include "tap_detector.h"
#endif
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

#ifdef MOTION_TEST
// Finds the motion sensor, then for two minutes shows the pull of gravity on
// each axis, how hard the last knock was, and the taps the detector saw at the
// default sensitivity. Every knock is also logged, to choose a good default.
static void runMotionTest() {
  String devices = audioScanI2c();
  Serial.printf("I2C devices: %s\n", devices.c_str());
  if (!motionBegin()) {
    showMessage("Motion", String("not found\n") + devices, colour::red());
    delay(8000);
    return;
  }
  char found[24];
  snprintf(found, sizeof(found), "found at\n0x%02X", motionAddress());
  showMessage("Motion", found, colour::green());
  delay(2000);

  tap::Detector detector; // sensitivity 5: 1.5 g
  const uint32_t testMs = 120000;
  const float barMaxG = 3.0f;
  const int barWidth = PANEL_RES_X - 4;
  int singles = 0, doubles = 0, errors = 0;
  float x = 0, y = 0, z = 0, held = 0;
  uint32_t heldAt = 0, flashUntil = 0, nextDraw = 0, nextLog = 0;
  uint16_t flashColour = 0;
  uint32_t start = millis(), nextSample = millis();

  while (millis() - start < testMs) {
    // 250 samples a second, as p64 reads it.
    while ((int32_t)(millis() - nextSample) < 0) delay(1);
    nextSample += 4;
    uint32_t now = millis();
    if (!motionRead(x, y, z)) {
      errors++;
      continue;
    }
    tap::Gesture gesture = detector.feed(now, x, y, z);
    if (gesture != tap::Gesture::None) {
      bool twice = gesture == tap::Gesture::Double;
      (twice ? doubles : singles)++;
      flashColour = twice ? display->color565(80, 200, 255) : colour::green();
      flashUntil = now + 600;
      Serial.printf("%s tap, peak %.2f g\n", twice ? "Double" : "Single", detector.lastTapPeak());
    }

    if ((int32_t)(now - nextLog) >= 0) {
      // Log any knock, even one too light to count, so the threshold can be chosen.
      float peak = detector.takePeak();
      if (peak >= 0.15f) Serial.printf("knock %.2f g\n", peak);
      if (peak > held || now - heldAt > 1500) {
        held = peak;
        heldAt = now;
      }
      nextLog = now + 100;
    }

    if ((int32_t)(now - nextDraw) < 0) continue;
    nextDraw = now + 80;
    char line[16];
    display->clearScreen();
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(colour::amber());
    display->setCursor(2, 2);
    display->print("Tap test");
    display->setTextColor(display->color565(150, 150, 150));
    const char *names = "XYZ";
    float axes[3] = {x, y, z};
    for (int i = 0; i < 3; i++) {
      snprintf(line, sizeof(line), "%c %+.2f", names[i], axes[i]);
      display->setCursor(2, 12 + i * 8);
      display->print(line);
    }
    // The last knock as a bar, 0 to 3 g, with a mark every half g and the
    // default threshold in amber.
    int bar = min(barWidth, (int)(held / barMaxG * barWidth));
    display->fillRect(2, 38, bar, 5, held >= detector.threshold() ? colour::green() : colour::red());
    for (int i = 1; i < 6; i++) {
      int at = 2 + i * barWidth / 6;
      display->drawPixel(at, 44, i == 3 ? colour::amber() : display->color565(90, 90, 90));
    }
    display->setCursor(2, 47);
    snprintf(line, sizeof(line), "pk %.2fg", held);
    display->print(line);
    display->setCursor(2, 56);
    snprintf(line, sizeof(line), "1x%d 2x%d", singles, doubles);
    display->print(line);
    if ((int32_t)(flashUntil - now) > 0) {
      display->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, flashColour);
    }
  }
  Serial.printf("Motion test done: %d single, %d double, %d read errors\n", singles, doubles, errors);
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
#ifdef MOTION_TEST
  runMotionTest();
#endif
}
