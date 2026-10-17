#include "slots/slot_makapix.h"

#include <WiFi.h>
#include <algorithm>
#include <vector>

#include "display/display.h"
#include "display/gif_player.h"
#include "app/memory_trace.h"
#include "app/status.h"
#include "app/telemetry.h"
#include "network/reporter.h"
#include "storage/art_cache.h"

static const uint32_t CODE_VALID_MS = 15 * 60 * 1000;
static const uint32_t CODE_SHOW_MS = 45000; // per turn, so other slots still get shown
static const uint32_t POLL_INTERVAL_MS = 5000;
static const uint32_t RATE_LIMIT_WAIT_MS = 60000;
static const uint32_t LINKED_SHOW_MS = 1500;
static const uint32_t LOAD_WAIT_MS = 15000;
static const uint32_t IDLE_UPDATE_MS = 100;
static const uint32_t VIEW_AFTER_MS = 2000; // shown this long, an artwork counts as seen
static const int MAX_LOAD_ATTEMPTS = 5;
static const uint32_t LOADER_STACK_BYTES = 16384; // TLS handshake plus JSON parsing
static const BaseType_t LOADER_CORE = 0;          // away from the display loop on core 1

enum class LoadStatus { Ready, Unauthorized, Refused, Failed };

static const uint32_t REFUSED_RETRY_MS = 5 * 60 * 1000; // a channel Makapix refused is tried again after this
static const size_t MAX_FEEDS = 8;                      // one per artwork slot in the longest rotation

struct LoadedArtwork {
  makapix::Artwork artwork;
  uint8_t *data;
  size_t size;
};

// One channel's place in its Makapix feed, and the artwork it has ready. Each
// channel in the rotation keeps its own, so taking turns between channels
// throws nothing away. The loader task owns a feed while it loads for it; the
// display loop owns it otherwise.
struct Feed {
  String spec;
  channels::Channel channel;
  std::vector<makapix::Artwork> queue;
  size_t queueIndex = 0;
  makapix::PageState page;
  bool ready = false; // the loader has finished: `status` and `pending` hold its result
  LoadStatus status = LoadStatus::Failed;
  LoadedArtwork pending = {};
  String problem;            // why the channel cannot play, while it cannot
  uint32_t retryAt = 0;      // after a refusal, when to ask Makapix again
  uint32_t lastUsed = 0;
};

static std::vector<Feed *> feeds;
static SemaphoreHandle_t feedsLock; // the API reads problems from another task
static Feed *loadingFeed = nullptr;
static SemaphoreHandle_t loaderDone;
static TaskHandle_t loader = nullptr;
static bool loading = false;

namespace {
struct FeedsGuard {
  FeedsGuard() { xSemaphoreTake(feedsLock, portMAX_DELAY); }
  ~FeedsGuard() { xSemaphoreGive(feedsLock); }
};
} // namespace

static void setProblem(Feed &feed, const String &problem) {
  FeedsGuard guard;
  feed.problem = problem;
}

// The next artwork from the card, when Makapix cannot supply one.
static bool loadFromCache(LoadedArtwork &loaded) {
  CachedArtwork cached;
  if (!artCacheTake(cached)) {
    return false;
  }
  makapix::Artwork artwork;
  artwork.postId = cached.postId;
  artwork.ownerHandle = cached.owner;
  artwork.dwellMs = cached.dwellMs;
  artwork.fromCache = true;
  loaded = {artwork, cached.data, cached.size};
  return true;
}

// Takes the next artwork from the feed's queue (fetching a new page when it
// runs out) and downloads it, skipping ones that fail to download. Without a
// connection, or when Makapix cannot be reached, it plays from the card
// instead. A channel Makapix refuses, or one with no art that fits, is not
// covered up with art from the card: the slot is skipped and says why.
static LoadStatus loadNext(Feed &feed, LoadedArtwork &loaded) {
  if (WiFi.status() != WL_CONNECTED) {
    return loadFromCache(loaded) ? LoadStatus::Ready : LoadStatus::Failed;
  }
  for (int attempt = 0; attempt < MAX_LOAD_ATTEMPTS; attempt++) {
    if (feed.queueIndex >= feed.queue.size()) {
      feed.queueIndex = 0;
      String problem;
      makapix::QueryStatus status = makapix::nextPage(feed.channel, feed.page, feed.queue, problem);
      if (status == makapix::QueryStatus::Unauthorized) {
        return LoadStatus::Unauthorized;
      }
      if (status == makapix::QueryStatus::Refused) {
        setProblem(feed, problem);
        return LoadStatus::Refused;
      }
      if (status == makapix::QueryStatus::Failed) {
        return loadFromCache(loaded) ? LoadStatus::Ready : LoadStatus::Failed;
      }
      if (feed.queue.empty()) {
        setProblem(feed, String("nothing in ") + channels::label(feed.channel).c_str() + " fits the panel");
        return LoadStatus::Refused;
      }
      Serial.printf("Queued %u artworks from %s\n", (unsigned)feed.queue.size(), feed.spec.c_str());
    }

    const makapix::Artwork &artwork = feed.queue[feed.queueIndex++];
    size_t size = 0;
    uint8_t *data = downloadGif(artwork.gifUrl, size);
    MEMORY_CHECK("download");
    if (!data) {
      continue;
    }
    // The vault sometimes answers with something that is not a GIF.
    if (size < 6 || memcmp(data, "GIF8", 4) != 0) {
      Serial.printf("Not a GIF, skipping: %s\n", artwork.gifUrl.c_str());
      free(data);
      continue;
    }
    telemetryNoteImage(size);
    artCacheStore(artwork.postId, artwork.ownerHandle, artwork.dwellMs, data, size);
    MEMORY_CHECK("cache store");
    loaded = {artwork, data, size};
    setProblem(feed, "");
    return LoadStatus::Ready;
  }
  return loadFromCache(loaded) ? LoadStatus::Ready : LoadStatus::Failed;
}

// One loader for the life of the board: it waits to be told to load, loads,
// and waits again. Making a task with a 16 KB stack for every artwork, as it
// once did, broke internal RAM into pieces too small for a TLS session.
static void loaderTask(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    Feed *feed = loadingFeed;
    feed->status = loadNext(*feed, feed->pending);
    xSemaphoreGive(loaderDone);
  }
}

// Loads the feed's next artwork in the background. The result is collected
// by taking loaderDone, and then marking the feed ready.
static void startLoading(Feed *feed) {
  loading = true;
  loadingFeed = feed;
  if (loader) {
    xTaskNotifyGive(loader);
  } else {
    feed->status = loadNext(*feed, feed->pending); // no loader task: load here instead
    xSemaphoreGive(loaderDone);
  }
}

// Waits up to `ms` for the loader to finish, and hands its result to its feed.
static bool collectLoader(uint32_t ms) {
  if (!loading) {
    return true;
  }
  if (xSemaphoreTake(loaderDone, pdMS_TO_TICKS(ms)) != pdTRUE) {
    return false;
  }
  loading = false;
  loadingFeed->ready = true;
  loadingFeed = nullptr;
  return true;
}

static void forgetFeed(Feed *feed) {
  if (feed->ready && feed->status == LoadStatus::Ready) {
    free(feed->pending.data);
  }
  delete feed;
}

// The feed for `spec`, made if it is new. Feeds of channels no longer in the
// rotation are dropped, least recently used first, beyond MAX_FEEDS.
static Feed *feedFor(const String &spec) {
  for (Feed *feed : feeds) {
    if (feed->spec == spec) {
      feed->lastUsed = millis();
      return feed;
    }
  }
  Feed *feed = new Feed();
  std::string error;
  channels::parse(spec.c_str(), feed->channel, error); // the rotation only holds valid ones
  feed->spec = spec;
  feed->lastUsed = millis();
  FeedsGuard guard;
  while (feeds.size() >= MAX_FEEDS) {
    Feed *oldest = nullptr;
    for (Feed *candidate : feeds) {
      if (candidate != loadingFeed && (!oldest || candidate->lastUsed < oldest->lastUsed)) {
        oldest = candidate;
      }
    }
    if (!oldest) {
      break;
    }
    feeds.erase(std::find(feeds.begin(), feeds.end(), oldest));
    forgetFeed(oldest);
  }
  feeds.push_back(feed);
  return feed;
}

String makapixChannelProblem(const String &spec) {
  FeedsGuard guard;
  for (Feed *feed : feeds) {
    if (feed->spec == spec) {
      return feed->problem;
    }
  }
  return "";
}

void MakapixSlot::begin() {
  loaderDone = xSemaphoreCreateBinary();
  feedsLock = xSemaphoreCreateMutex();
  makapix::begin();
  // Made once, early, while internal RAM is still in one piece.
  if (xTaskCreatePinnedToCore(loaderTask, "loader", LOADER_STACK_BYTES, nullptr, 1, &loader, LOADER_CORE) == pdPASS) {
    telemetryRegisterTask("loader", loader, LOADER_STACK_BYTES);
  } else {
    loader = nullptr;
  }
}

void MakapixSlot::useChannel(const String &spec) { feed = feedFor(spec.isEmpty() ? String("all") : spec); }

bool MakapixSlot::start() {
  mode = Mode::Idle;
  if (requestedPost) {
    uint32_t postId = requestedPost;
    requestedPost = 0;
    return showFromCard(postId);
  }
  if (!feed) {
    useChannel("all");
  }
  if (WiFi.status() != WL_CONNECTED) {
    // Offline: the loader can still play from the card.
    return (loading || feed->ready || artCacheAvailable()) ? startPlaying() : false;
  }
  return makapix::isRegistered() ? startPlaying() : startRegistration();
}

// Shows a registration code for the user to enter on makapix.club. The same
// code is reused on later turns until it expires.
bool MakapixSlot::startRegistration() {
  if (code.isEmpty() || millis() - codeIssuedAt >= CODE_VALID_MS) {
    if (!makapix::provision(code)) {
      code = "";
      return false;
    }
    codeIssuedAt = millis();
    Serial.printf("Registration code: %s\n", code.c_str());
  }

  mode = Mode::Registering;
  startedAt = millis();
  lastPollAt = millis();
  pollIntervalMs = POLL_INTERVAL_MS;
  linked = false;
  showRegistrationCode(code);
  return true;
}

bool MakapixSlot::startPlaying() {
  Feed &current = *feed;
  // A refused channel waits before Makapix is asked again; its turn is skipped.
  if (!current.ready && current.retryAt && (int32_t)(millis() - current.retryAt) < 0) {
    return false;
  }
  // The loader may be busy with this channel or another; let it finish first.
  if (!collectLoader(LOAD_WAIT_MS)) {
    return false; // still downloading; it will be ready on a later turn
  }
  if (!current.ready) {
    startLoading(&current);
    if (!collectLoader(LOAD_WAIT_MS)) {
      return false;
    }
  }
  current.ready = false;

  if (current.status == LoadStatus::Unauthorized) {
    // Token revoked or device removed on the site: register again.
    Serial.println("Token rejected, registering again");
    makapix::forget();
    for (Feed *each : feeds) {
      each->queue.clear();
      each->queueIndex = 0;
      each->page = makapix::PageState();
    }
    return startRegistration();
  }
  if (current.status == LoadStatus::Refused) {
    current.retryAt = millis() + REFUSED_RETRY_MS;
    Serial.printf("Channel %s skipped: %s\n", current.spec.c_str(), makapixChannelProblem(current.spec).c_str());
    return false;
  }
  current.retryAt = 0;
  if (current.status == LoadStatus::Failed) {
    return false;
  }

  LoadedArtwork next = current.pending;
  current.pending = {};
  startLoading(&current); // preload this channel's next one while this one plays
  if (!play(next)) {
    return false;
  }
  if (!next.artwork.fromCache) {
    playingChannel = &current.channel;
  }
  return true;
}

// One artwork asked for from the gallery. The preloaded one stays waiting for
// the next turn.
bool MakapixSlot::showFromCard(uint32_t postId) {
  LoadedArtwork requested;
  CachedArtwork cached;
  if (!artCacheTakePost(postId, cached)) {
    Serial.printf("Post %u is no longer on the card\n", (unsigned)postId);
    return false;
  }
  requested.artwork.postId = cached.postId;
  requested.artwork.ownerHandle = cached.owner;
  requested.artwork.dwellMs = cached.dwellMs;
  requested.artwork.fromCache = true;
  requested.data = cached.data;
  requested.size = cached.size;
  if (!play(requested)) {
    return false;
  }
  this->requested = true;
  return true;
}

bool MakapixSlot::play(const LoadedArtwork &current) {
  MEMORY_CHECK("play");
  Serial.printf("Playing post %u (%u bytes%s)\n", (unsigned)current.artwork.postId, (unsigned)current.size,
                current.artwork.fromCache ? ", from the card" : "");
  if (!gifOpen(current.data, current.size)) {
    free(current.data);
    return false;
  }

  // The panel fetches over plain HTTP; other clients get the HTTPS address.
  String shareUrl = current.artwork.gifUrl;
  if (shareUrl.startsWith("http://")) {
    shareUrl = "https://" + shareUrl.substring(7);
  }
  statusSetArtwork(current.artwork.postId, current.artwork.ownerHandle, shareUrl, current.artwork.fromCache,
                   current.artwork.fromCache || !feed ? "" : channels::label(feed->channel).c_str());
  playingChannel = nullptr; // set by the caller when it came from a channel
  mode = Mode::Playing;
  startedAt = millis();
  postId = current.artwork.postId;
  requested = false;
  viewNoted = false;
  data = current.data;
  dwellMs = current.artwork.dwellMs;
  framesThisLoop = 0;
  loopEnded = false;
  still = false;
  failed = false;
  return true;
}

void MakapixSlot::shown() { startedAt = millis(); }

uint32_t MakapixSlot::update() {
  switch (mode) {
    case Mode::Registering:
      return updateRegistration();
    case Mode::Playing:
      return updatePlaying();
    default:
      return IDLE_UPDATE_MS;
  }
}

uint32_t MakapixSlot::updateRegistration() {
  if (linked || millis() - lastPollAt < pollIntervalMs) {
    return IDLE_UPDATE_MS;
  }
  lastPollAt = millis();

  makapix::LinkStatus status = makapix::pollCredentials();
  pollIntervalMs =
      status == makapix::LinkStatus::RateLimited ? RATE_LIMIT_WAIT_MS : POLL_INTERVAL_MS;
  if (status == makapix::LinkStatus::Linked) {
    Serial.println("Device linked");
    code = "";
    linked = true;
    linkedAt = millis();
    showMessage("Makapix", "linked!", display->color565(0, 255, 0));
  }
  return IDLE_UPDATE_MS;
}

uint32_t MakapixSlot::updatePlaying() {
  if (!viewNoted && !failed && millis() - startedAt >= VIEW_AFTER_MS) {
    viewNoted = true;
    if (playingChannel) {
      const char *hashtag = playingChannel->kind == channels::Kind::Hashtag ? playingChannel->value.c_str() : "";
      reporterNoteView(postId, requested, channels::makapixName(playingChannel->kind), hashtag);
    } else {
      reporterNoteView(postId, requested);
    }
  }
  if (failed || (still && framesThisLoop > 0)) {
    return IDLE_UPDATE_MS; // nothing left to draw
  }

  if (loopEnded) {
    gifRestart();
    framesThisLoop = 0;
    loopEnded = false;
  }

  int frameMs = 0;
  int more = gifPlayFrame(frameMs);
  if (more < 0) {
    failed = true;
    return 0;
  }
  framesThisLoop++;

  if (more == 0) {
    if (framesThisLoop == 1) {
      still = true;
      return IDLE_UPDATE_MS;
    }
    loopEnded = true; // rewound on the next call, after this frame has had its time
  }
  return frameMs;
}

bool MakapixSlot::finished() const {
  switch (mode) {
    case Mode::Registering:
      return linked ? millis() - linkedAt >= LINKED_SHOW_MS : millis() - startedAt >= CODE_SHOW_MS;
    case Mode::Playing:
      // Animations end on a loop boundary, never part-way through.
      return failed || (millis() - startedAt >= dwellMs && (still || loopEnded));
    default:
      return true;
  }
}

void MakapixSlot::redraw() {
  if (mode == Mode::Registering && !linked) {
    showRegistrationCode(code);
  } else if (mode == Mode::Playing && !failed) {
    display->clearScreen();
    gifRestart();
    framesThisLoop = 0;
    loopEnded = false;
    still = false;
  }
}

void MakapixSlot::stop() {
  if (mode == Mode::Playing) {
    gifClose();
    free(data);
    data = nullptr;
    statusClearArtwork();
  }
  mode = Mode::Idle;
}
