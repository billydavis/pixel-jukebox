#pragma once

#include "network/makapix.h"
#include "slots/slot.h"

struct LoadedArtwork;
struct Feed;

// Why the channel `spec` cannot play right now, or empty. Safe from any task.
String makapixChannelProblem(const String &spec);

// Plays one Makapix artwork per turn, preloading the next in the background.
// While the device is not registered it shows the registration code instead.
class MakapixSlot : public Slot {
 public:
  void begin();

  // The next start() shows this artwork instead, once: from the card when it
  // is there, otherwise fetched from Makapix.
  void request(uint32_t postId) { requestedPost = postId; }

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
  bool play(const LoadedArtwork &artwork);
  uint32_t updateRegistration();
  uint32_t updatePlaying();

  Mode mode = Mode::Idle;
  Feed *feed = nullptr; // the channel in use
  uint32_t requestedPost = 0;
  uint32_t startedAt = 0;

  // Registration
  String code;
  uint32_t codeIssuedAt = 0;
  uint32_t lastPollAt = 0;
  uint32_t pollIntervalMs = 0;
  bool linked = false;
  uint32_t linkedAt = 0;

  // Playback
  uint8_t *data = nullptr;
  uint32_t postId = 0;
  bool requested = false; // asked for from the gallery
  bool viewNoted = false;
  const channels::Channel *playingChannel = nullptr; // where the artwork came from; null from the card
  uint32_t dwellMs = 0;
  int framesThisLoop = 0;
  bool loopEnded = false;
  bool still = false;
  bool failed = false;
};
