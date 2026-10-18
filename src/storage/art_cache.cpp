#include "storage/art_cache.h"

#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

#include "app/telemetry.h"
#include "cache_index.h"
#include "gif_pad.h"
#include "app/memory_trace.h"
#include "storage/card.h"

static const char *ART_DIR = CARD_ROOT "/art";
static const char *LOG_FILE = CARD_ROOT "/cache.log";
static const char *LOG_TEMP = CARD_ROOT "/cache.tmp";

static const int MAX_ARTWORKS = 1500;
static const uint64_t MAX_BYTES = 1024ULL << 20;   // never more than 1 GB
static const uint64_t KEEP_FREE_BYTES = 64ULL << 20; // and always leave this much of the card
static const int MAX_LINE = 96;

static cache::Entry *storage = nullptr;
static cache::Index *catalogue = nullptr;
static uint64_t budget = 0;
static uint32_t stored = 0; // artworks written since start-up
static uint32_t played = 0; // artworks played from the card since start-up
static std::vector<uint32_t> order; // the current shuffled pass
static size_t orderPosition = 0;
static String problem;

namespace {
// The cache's index and its files are covered by the card lock, which every
// task using the card takes (see card.h).
struct Guard {
  CardLock card;
};
} // namespace

static String artPath(uint32_t postId) { return String(ART_DIR) + "/" + postId + ".gif"; }

// Deletes files in the art folder that the index does not list: half-written
// files from a power cut, and anything left over when the index was full.
// Squares the catalogue with the art folder in one pass over the folder:
// files the catalogue does not know (a write cut short, say) are deleted,
// and entries whose file has gone (a card edited on a computer) are dropped.
//
// One pass matters. FAT folders have no index, so looking each file up on
// its own reads the folder from the top every time: with 1,500 artworks that
// took 48 s of every start-up (2026-10-18).
static int reconcileWithFolder() {
  DIR *dir = opendir(ART_DIR);
  if (!dir) {
    return 0;
  }
  std::vector<uint32_t> present;
  present.reserve(catalogue->count());
  std::vector<String> strays;
  while (struct dirent *item = readdir(dir)) {
    String name = item->d_name;
    if (name == "." || name == "..") {
      continue;
    }
    uint32_t postId = strtoul(name.c_str(), nullptr, 10);
    if (!name.endsWith(".gif") || name.indexOf('.') != name.lastIndexOf('.') || !catalogue->find(postId)) {
      strays.push_back(String(ART_DIR) + "/" + name);
    } else {
      present.push_back(postId);
    }
  }
  closedir(dir);
  for (const String &path : strays) {
    remove(path.c_str());
  }
  std::sort(present.begin(), present.end());
  for (int i = catalogue->count() - 1; i >= 0; i--) {
    uint32_t postId = catalogue->at(i).postId;
    if (!std::binary_search(present.begin(), present.end(), postId)) {
      catalogue->remove(postId);
    }
  }
  return strays.size();
}

static bool appendLog(const std::string &line) {
  FILE *file = fopen(LOG_FILE, "a");
  if (!file) {
    return false;
  }
  bool ok = fputs(line.c_str(), file) >= 0;
  return fclose(file) == 0 && ok;
}

// Rewrites the log with one record per artwork, dropping the history.
static void compactLog() {
  FILE *file = fopen(LOG_TEMP, "w");
  if (!file) {
    return;
  }
  bool ok = true;
  for (int i = 0; i < catalogue->count() && ok; i++) {
    ok = fputs(cache::Index::addLine(catalogue->at(i)).c_str(), file) >= 0;
  }
  if (fclose(file) == 0 && ok) {
    remove(LOG_FILE);
    rename(LOG_TEMP, LOG_FILE);
  } else {
    remove(LOG_TEMP);
  }
}

void artCacheBegin() {
  if (!cardReady()) {
    problem = "no card";
    return;
  }
  storage = (cache::Entry *)ps_calloc(MAX_ARTWORKS, sizeof(cache::Entry));
  if (!storage) {
    problem = "out of memory";
    return;
  }
  catalogue = new cache::Index(storage, MAX_ARTWORKS);
  mkdir(ART_DIR, 0777);

  int lines = 0;
  int damaged = 0;
  FILE *file = fopen(LOG_FILE, "r");
  if (file) {
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file)) {
      lines++;
      damaged += catalogue->apply(line) ? 0 : 1;
    }
    fclose(file);
  }

  telemetryBootMark("cache log read");
  int strays = reconcileWithFolder();
  if (lines > catalogue->count() * 2 + 64 || damaged > 0 || strays > 0) {
    compactLog();
  }

  JsonDocument card;
  cardReport(card.to<JsonObject>());
  uint64_t room = card["freeBytes"].as<uint64_t>() + catalogue->totalBytes();
  budget = room > KEEP_FREE_BYTES ? min(MAX_BYTES, room - KEEP_FREE_BYTES) : 0;
  Serial.printf("Cache: %d artworks, %llu KB, room for %llu MB, %d stray files removed%s\n",
                catalogue->count(), catalogue->totalBytes() >> 10, budget >> 20, strays,
                damaged ? ", damaged log lines skipped" : "");
}

// Removes the artwork that has gone longest unplayed. Called with the lock held.
static bool evictOldest() {
  int oldest = catalogue->oldest();
  if (oldest < 0) {
    return false;
  }
  uint32_t postId = catalogue->at(oldest).postId;
  remove(artPath(postId).c_str());
  catalogue->remove(postId);
  appendLog(cache::Index::removeLine(postId));
  return true;
}

void artCacheStore(uint32_t postId, const String &owner, uint32_t dwellMs, const uint8_t *data, size_t size) {
  if (!catalogue || budget == 0 || size > budget) {
    return;
  }
  Guard guard;
  cache::Entry entry = cache::Index::make(postId, catalogue->nextSeq(), dwellMs, size, owner.c_str());

  // Already cached: only note that it played again.
  if (catalogue->find(postId)) {
    catalogue->put(entry);
    appendLog(cache::Index::addLine(entry));
    return;
  }

  while (catalogue->count() >= catalogue->capacity() || catalogue->totalBytes() + size > budget) {
    if (!evictOldest()) {
      return;
    }
  }

  // Written under a temporary name first, so a power cut cannot leave a
  // truncated artwork that the log says is complete.
  String path = artPath(postId);
  String temp = path + ".tmp";
  FILE *file = fopen(temp.c_str(), "wb");
  if (!file) {
    problem = "cannot write to the card";
    return;
  }
  bool ok = fwrite(data, 1, size, file) == size;
  ok = fclose(file) == 0 && ok;
  if (!ok || rename(temp.c_str(), path.c_str()) != 0) {
    remove(temp.c_str());
    problem = "cannot write to the card";
    return;
  }
  catalogue->put(entry);
  appendLog(cache::Index::addLine(entry));
  stored++;
  problem = "";
}

bool artCacheAvailable() {
  if (!catalogue) {
    return false;
  }
  Guard guard;
  return catalogue->count() > 0;
}

// Drops an artwork the card could not give back. Called with the lock held.
static void forget(uint32_t postId) {
  remove(artPath(postId).c_str());
  catalogue->remove(postId);
  appendLog(cache::Index::removeLine(postId));
}

// Reads an artwork's file into PSRAM, with room for gifPadSmall(). Null when
// it is missing, too large or not a GIF. Called with the lock held.
static uint8_t *readArt(uint32_t postId, size_t &size) {
  FILE *file = fopen(artPath(postId).c_str(), "rb");
  if (!file) {
    return nullptr;
  }
  uint8_t *data = nullptr;
  size = 0;
  if (fseek(file, 0, SEEK_END) == 0) {
    long length = ftell(file);
    if (length > 6 && (uint64_t)length <= budget) {
      size = length;
      data = (uint8_t *)ps_malloc(size + GIF_PAD_BYTES);
      if (data && (fseek(file, 0, SEEK_SET) != 0 || fread(data, 1, size, file) != size)) {
        free(data);
        data = nullptr;
      }
    }
  }
  fclose(file);
  MEMORY_CHECK("card read");
  if (data && memcmp(data, "GIF8", 4) != 0) {
    free(data);
    data = nullptr;
  }
  return data;
}

// Hands out an artwork that has just been read, noting the play. Called with
// the lock held.
static void markPlayed(cache::Entry entry, uint8_t *data, size_t size, CachedArtwork &artwork) {
  // Playing counts as use, so it is not the next to be removed.
  entry.seq = catalogue->nextSeq();
  catalogue->put(entry);
  appendLog(cache::Index::addLine(entry));
  played++;
  artwork = {entry.postId, entry.owner, entry.dwellMs, data, gifPadSmall(data, size)};
}

// Every cached artwork once, in random order, then a fresh order.
static bool nextInOrder(uint32_t &postId) {
  for (int pass = 0; pass < 2; pass++) {
    while (orderPosition < order.size()) {
      postId = order[orderPosition++];
      if (catalogue->find(postId)) {
        return true;
      }
    }
    order.clear();
    for (int i = 0; i < catalogue->count(); i++) {
      order.push_back(catalogue->at(i).postId);
    }
    for (size_t i = order.size(); i > 1; i--) {
      std::swap(order[i - 1], order[esp_random() % i]);
    }
    orderPosition = 0;
  }
  return false;
}

bool artCacheTake(CachedArtwork &artwork) {
  if (!catalogue) {
    return false;
  }
  Guard guard;
  for (int attempt = 0; attempt < 3; attempt++) {
    uint32_t postId;
    if (!nextInOrder(postId)) {
      return false;
    }
    size_t size;
    uint8_t *data = readArt(postId, size);
    if (!data) {
      Serial.printf("Cache: post %u could not be read, removing it\n", (unsigned)postId);
      forget(postId);
      continue;
    }
    markPlayed(*catalogue->find(postId), data, size, artwork);
    return true;
  }
  return false;
}

bool artCacheTakePost(uint32_t postId, CachedArtwork &artwork) {
  if (!catalogue) {
    return false;
  }
  Guard guard;
  const cache::Entry *entry = catalogue->find(postId);
  if (!entry) {
    return false;
  }
  size_t size;
  uint8_t *data = readArt(postId, size);
  if (!data) {
    Serial.printf("Cache: post %u could not be read, removing it\n", (unsigned)postId);
    forget(postId);
    return false;
  }
  markPlayed(*entry, data, size, artwork);
  return true;
}

bool artCacheReadFile(uint32_t postId, uint8_t *&data, size_t &size) {
  if (!catalogue) {
    return false;
  }
  Guard guard;
  if (!catalogue->find(postId)) {
    return false;
  }
  data = readArt(postId, size);
  return data != nullptr;
}

bool artCacheHas(uint32_t postId) {
  if (!catalogue) {
    return false;
  }
  Guard guard;
  return catalogue->find(postId) != nullptr;
}

void artCacheList(uint32_t offset, uint32_t limit, JsonObject out) {
  JsonArray items = out["artworks"].to<JsonArray>();
  out["offset"] = offset;
  if (!catalogue) {
    out["total"] = 0;
    return;
  }
  Guard guard;
  std::vector<int> byRecent(catalogue->count());
  for (int i = 0; i < catalogue->count(); i++) {
    byRecent[i] = i;
  }
  std::sort(byRecent.begin(), byRecent.end(),
            [](int a, int b) { return catalogue->at(a).seq > catalogue->at(b).seq; });
  out["total"] = catalogue->count();
  for (uint32_t i = offset; i < byRecent.size() && i < offset + limit; i++) {
    const cache::Entry &entry = catalogue->at(byRecent[i]);
    JsonObject item = items.add<JsonObject>();
    item["postId"] = entry.postId;
    item["owner"] = entry.owner;
    item["bytes"] = entry.bytes;
    item["dwellMs"] = entry.dwellMs;
  }
}

bool artCacheRemove(uint32_t postId) {
  if (!catalogue) {
    return false;
  }
  Guard guard;
  if (!catalogue->find(postId)) {
    return false;
  }
  forget(postId);
  return true;
}

int artCacheClear() {
  if (!catalogue) {
    return 0;
  }
  Guard guard;
  int removed = catalogue->count();
  for (int i = 0; i < removed; i++) {
    remove(artPath(catalogue->at(i).postId).c_str());
  }
  catalogue->clear();
  remove(LOG_FILE);
  reconcileWithFolder(); // the catalogue is empty, so anything left is deleted
  order.clear();
  orderPosition = 0;
  Serial.printf("Cache: cleared %d artworks\n", removed);
  return removed;
}

void artCacheReport(JsonObject report) {
  if (!catalogue) {
    report["enabled"] = false;
    report["problem"] = problem;
    return;
  }
  Guard guard;
  report["enabled"] = budget > 0;
  report["artworks"] = catalogue->count();
  report["bytes"] = catalogue->totalBytes();
  report["budgetBytes"] = budget;
  report["maxArtworks"] = catalogue->capacity();
  report["storedSinceStart"] = stored;
  report["playedSinceStart"] = played;
  if (!problem.isEmpty()) {
    report["problem"] = problem;
  }
}
