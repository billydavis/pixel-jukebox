#pragma once

#include "network/makapix.h"
#include "slots/slot.h"

struct LoadedArtwork;
struct Feed;

// Why the channel `spec` cannot play right now, or empty. Safe from any task.
String makapixChannelProblem(const String &spec);

// Plays one Makapix artwork per turn, preloading the next in the background.
// While the device is not registered it shows the registration code instead,
// every other turn, and starter art in between. With nothing else to play it
// plays starter art (storage/starter_art.h).
class MakapixSlot : public Slot {
 public:
  void begin();

  // Called every 50 ms while start() waits for a download (up to 15 s), so
  // the display loop can still act on commands that need no slot, such as
  // brightness. Set once by main.cpp.
  static void (*whileWaiting)();

  // The next start() shows this artwork instead, once: from the card when it
  // is there, otherwise fetched from Makapix.
  void request(uint32_t postId) { requestedPost = postId; }
  // The same for an upload (storage/uploads.h).
  void requestUpload(uint32_t uploadId) { requestedUpload = uploadId; }
  // The same for a piece of starter art (storage/starter_art.h).
  void requestStarter(uint16_t id) { requestedStarter = id; }

  // The channel the next start() plays from, as a channels.h string.
  void useChannel(const String &spec);

  const char *name() const override { return "makapix"; }
  bool start() override;
  void shown() override;
  uint32_t update() override;
  bool finished() const override;
  void redraw() override;
  void stop() override;

 private:
  enum class Mode { Idle, Registering, Playing };

  bool startRegistration();
  bool startPlaying();
  bool showRequested(uint32_t postId);
  bool showUpload(uint32_t uploadId);
  bool showStarter(uint16_t id);
  bool playStarter();
  bool play(const LoadedArtwork &artwork);
  uint32_t updateRegistration();
  uint32_t updatePlaying();

  Mode mode = Mode::Idle;
  Feed *feed = nullptr; // the channel in use
  uint32_t requestedPost = 0;
  uint32_t requestedUpload = 0;
  uint16_t requestedStarter = 0;
  uint32_t startedAt = 0;

  // Registration
  String code;
  uint32_t codeIssuedAt = 0;
  uint32_t lastPollAt = 0;
  uint32_t pollIntervalMs = 0;
  bool linked = false;
  uint32_t linkedAt = 0;
  bool codeShownLast = false; // the last unlinked turn showed the code, so starter art is next

  // Playback
  uint8_t *data = nullptr;
  uint32_t postId = 0;
  uint32_t uploadId = 0; // playing an upload: no view is reported
  uint16_t starterId = 0; // playing starter art: no view is reported either
  bool starterFallback = false; // set before play() for status: shown for want of Makapix art
  bool requested = false; // asked for from the gallery
  bool viewNoted = false;
  const channels::Channel *playingChannel = nullptr; // where the artwork came from; null from the card
  uint32_t dwellMs = 0;
  int framesThisLoop = 0;
  bool loopEnded = false;
  bool still = false;
  bool failed = false;
};
