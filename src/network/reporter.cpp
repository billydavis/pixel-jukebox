#include "network/reporter.h"

#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include "app/telemetry.h"
#include "likes_book.h"
#include "network/makapix.h"
#include "storage/card.h"
#include "view_queue.h"

static const uint32_t STACK_BYTES = 12288; // a TLS handshake and a small JSON body
static const BaseType_t CORE = 0;          // away from the display loop on core 1
static const uint32_t VIEW_GAP_MS = 5500;  // Makapix accepts one view every 5 s
static const uint32_t RETRY_FAILED_MS = 30000;
static const uint32_t RETRY_UNAUTHORIZED_MS = 60000;
static const uint32_t RETRY_RATE_LIMITED_MS = 10000;
static const int REWRITE_EVERY = 10; // views sent between rewrites of the backlog file
static const time_t CLOCK_SET_AFTER = 1700000000; // 2023: before that, the clock was never set
static const char *BACKLOG_FILE = CARD_ROOT "/views.log";
static const char *BACKLOG_TEMP = CARD_ROOT "/views.tmp";

static SemaphoreHandle_t lock;
static TaskHandle_t task = nullptr;
static Preferences prefs;
static likes::Book book;
static views::Queue backlog;
static size_t unsaved = 0; // views at the back of the backlog not yet on the card
static bool viewsEnabled = true;

// Counts since start-up, for the API.
static uint32_t viewsSent = 0, viewsRefused = 0, viewsRateLimited = 0, viewsDropped = 0;
static uint32_t likesSent = 0, likesRefused = 0;
static String lastProblem;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static void wake() {
  if (task) {
    xTaskNotifyGive(task);
  }
}

// Called with the lock held.
static void saveLikes() {
  std::string bytes = book.save();
  prefs.putBytes("likes", bytes.data(), bytes.size());
}

// --- the backlog of views on the card -----------------------------------------

static void loadBacklog() {
  if (!cardReady()) {
    return;
  }
  FILE *file = fopen(BACKLOG_FILE, "r");
  if (!file) {
    return;
  }
  char line[48];
  int damaged = 0;
  Guard guard;
  while (fgets(line, sizeof(line), file)) {
    views::View view;
    if (views::fromLine(line, view)) {
      backlog.push(view);
    } else {
      damaged++;
    }
  }
  fclose(file);
  Serial.printf("Views: %u waiting from before the restart%s\n", (unsigned)backlog.size(),
                damaged ? ", damaged lines skipped" : "");
}

// Appends the views noted since the last save.
static void saveNewViews() {
  if (!cardReady()) {
    return;
  }
  std::string lines;
  {
    Guard guard;
    if (unsaved == 0) {
      return;
    }
    for (size_t i = backlog.size() - min(unsaved, backlog.size()); i < backlog.size(); i++) {
      lines += views::toLine(backlog.at(i));
    }
    unsaved = 0;
  }
  FILE *file = fopen(BACKLOG_FILE, "a");
  if (file) {
    fputs(lines.c_str(), file);
    fclose(file);
  }
}

// Writes the backlog as it now stands, or removes the file when it is empty.
static void rewriteBacklog() {
  if (!cardReady()) {
    return;
  }
  std::string lines;
  {
    Guard guard;
    for (size_t i = 0; i < backlog.size(); i++) {
      lines += views::toLine(backlog.at(i));
    }
    unsaved = 0;
  }
  if (lines.empty()) {
    remove(BACKLOG_FILE);
    return;
  }
  FILE *file = fopen(BACKLOG_TEMP, "w");
  if (!file) {
    return;
  }
  bool ok = fputs(lines.c_str(), file) >= 0;
  if (fclose(file) == 0 && ok) {
    remove(BACKLOG_FILE);
    rename(BACKLOG_TEMP, BACKLOG_FILE);
  } else {
    remove(BACKLOG_TEMP);
  }
}

// --- sending -----------------------------------------------------------------

static uint32_t retryAt = 0;
static uint32_t lastViewAt = 0;
static int sentSinceRewrite = 0;

static void waitBeforeRetrying(uint32_t ms, const char *problem) {
  retryAt = millis() + ms;
  Guard guard;
  lastProblem = problem;
}

static const char *problemFor(makapix::SendStatus status) {
  return status == makapix::SendStatus::Unauthorized ? "Makapix did not accept the panel's token"
                                                     : "could not reach Makapix";
}

// Sends the oldest change to a like. False when there was none.
static bool sendNextLike() {
  uint32_t postId;
  bool liked;
  {
    Guard guard;
    if (!book.nextPending(postId, liked)) {
      return false;
    }
  }
  makapix::SendStatus status = makapix::setLiked(postId, liked);
  Guard guard;
  switch (status) {
    case makapix::SendStatus::Ok:
      book.confirmed(postId, liked);
      likesSent++;
      lastProblem = "";
      Serial.printf("Makapix: %s post %u\n", liked ? "liked" : "unliked", (unsigned)postId);
      break;
    case makapix::SendStatus::Gone:
      book.drop(postId);
      likesRefused++;
      break;
    case makapix::SendStatus::RateLimited:
      retryAt = millis() + RETRY_RATE_LIMITED_MS;
      return true;
    default:
      retryAt = millis() + (status == makapix::SendStatus::Unauthorized ? RETRY_UNAUTHORIZED_MS : RETRY_FAILED_MS);
      lastProblem = problemFor(status);
      return true;
  }
  saveLikes();
  return true;
}

static void sendNextView() {
  if (millis() - lastViewAt < VIEW_GAP_MS) {
    return;
  }
  views::View view;
  {
    Guard guard;
    if (backlog.empty()) {
      return;
    }
    view = backlog.front();
  }
  lastViewAt = millis();
  makapix::SendStatus status = makapix::reportView(view.postId, views::isoTime(view.unixSeconds).c_str(), view.requested);
  if (status == makapix::SendStatus::RateLimited) {
    Guard guard;
    viewsRateLimited++;
    return; // try again after the gap
  }
  if (status == makapix::SendStatus::Failed || status == makapix::SendStatus::Unauthorized) {
    waitBeforeRetrying(status == makapix::SendStatus::Unauthorized ? RETRY_UNAUTHORIZED_MS : RETRY_FAILED_MS,
                       problemFor(status));
    return;
  }
  bool empty;
  {
    Guard guard;
    // Turning reports off may have emptied the backlog meanwhile.
    if (!backlog.empty() && backlog.front().postId == view.postId &&
        backlog.front().unixSeconds == view.unixSeconds) {
      backlog.pop();
    }
    (status == makapix::SendStatus::Ok ? viewsSent : viewsRefused)++;
    lastProblem = "";
    empty = backlog.empty();
  }
  if (empty || ++sentSinceRewrite >= REWRITE_EVERY) {
    sentSinceRewrite = 0;
    rewriteBacklog();
  }
}

static void reporterTask(void *) {
  loadBacklog();
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    saveNewViews();
    if (WiFi.status() != WL_CONNECTED || !makapix::isRegistered() || (int32_t)(millis() - retryAt) < 0) {
      continue;
    }
    if (!sendNextLike()) {
      sendNextView();
    }
  }
}

// --- the rest of the firmware ---------------------------------------------------

void reporterBegin() {
  lock = xSemaphoreCreateMutex();
  prefs.begin("reports", false);
  viewsEnabled = prefs.getBool("views", true);
  size_t size = prefs.getBytesLength("likes");
  if (size) {
    std::string bytes(size, '\0');
    prefs.getBytes("likes", &bytes[0], size);
    if (!book.load(bytes)) {
      Serial.println("Likes: saved list damaged, starting afresh");
    }
  }
  Serial.printf("Likes: %d liked, %d waiting to send. View reports %s\n", book.count(), book.pending(),
                viewsEnabled ? "on" : "off");
  xTaskCreatePinnedToCore(reporterTask, "reporter", STACK_BYTES, nullptr, 1, &task, CORE);
  telemetryRegisterTask("reporter", task, STACK_BYTES);
}

void reporterNoteView(uint32_t postId, bool requested) {
  {
    Guard guard;
    if (!viewsEnabled) {
      return;
    }
    time_t now = time(nullptr);
    if (!backlog.push({postId, now > CLOCK_SET_AFTER ? (uint32_t)now : 0, requested})) {
      viewsDropped++;
    }
    unsaved = min(unsaved + 1, backlog.size());
  }
  wake();
}

bool reporterViewsEnabled() {
  Guard guard;
  return viewsEnabled;
}

void reporterSetViewsEnabled(bool enabled) {
  {
    Guard guard;
    if (enabled == viewsEnabled) {
      return;
    }
    viewsEnabled = enabled;
    prefs.putBool("views", enabled);
    if (!enabled) {
      backlog.clear();
      unsaved = 0;
    }
  }
  if (!enabled && cardReady()) {
    remove(BACKLOG_FILE);
  }
}

bool reporterLiked(uint32_t postId) {
  Guard guard;
  return book.liked(postId);
}

void reporterSetLiked(uint32_t postId, bool liked) {
  {
    Guard guard;
    if (book.liked(postId) == liked) {
      return;
    }
    book.set(postId, liked);
    saveLikes();
  }
  wake();
}

std::vector<uint32_t> reporterLikes() {
  Guard guard;
  return book.list();
}

void reporterReportViews(JsonObject report) {
  Guard guard;
  report["report"] = viewsEnabled;
  report["waiting"] = backlog.size();
  report["sentSinceStart"] = viewsSent;
  report["refusedSinceStart"] = viewsRefused;
  report["rateLimitedSinceStart"] = viewsRateLimited;
  report["droppedSinceStart"] = viewsDropped;
  if (!lastProblem.isEmpty()) {
    report["problem"] = lastProblem;
  }
}

void reporterReportLikes(JsonObject report) {
  Guard guard;
  report["count"] = book.count();
  report["waiting"] = book.pending();
  report["sentSinceStart"] = likesSent;
  report["refusedSinceStart"] = likesRefused;
  if (!lastProblem.isEmpty()) {
    report["problem"] = lastProblem;
  }
}
