#include "cache_index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace cache {

Index::Index(Entry *storage, int capacity) : entries(storage), limit(capacity) {}

void Index::clear() { used = 0; }

// Reads one unsigned 32-bit number and moves past it; false if there is none.
static bool readNumber(const char *&p, uint32_t &value) {
  while (*p == ' ') {
    p++;
  }
  if (*p < '0' || *p > '9') {
    return false;
  }
  char *end = nullptr;
  unsigned long long parsed = strtoull(p, &end, 10);
  if (end == p || parsed > 0xFFFFFFFFULL || (*end != ' ' && *end != '\0' && *end != '\n' && *end != '\r')) {
    return false;
  }
  value = (uint32_t)parsed;
  p = end;
  return true;
}

bool Index::apply(const char *line) {
  const char *p = line;
  char kind = *p++;
  if (kind == 'R') {
    uint32_t postId;
    if (!readNumber(p, postId)) {
      return false;
    }
    remove(postId);
    return true;
  }
  if (kind != 'A') {
    return false;
  }
  uint32_t postId, seq, dwellMs, bytes;
  if (!readNumber(p, postId) || !readNumber(p, seq) || !readNumber(p, dwellMs) || !readNumber(p, bytes)) {
    return false;
  }
  while (*p == ' ') {
    p++;
  }
  char owner[OWNER_CHARS] = {};
  int length = 0;
  while (*p && *p != '\n' && *p != '\r' && length < OWNER_CHARS - 1) {
    owner[length++] = *p++;
  }
  if (length == 0 || postId == 0) {
    return false; // every line is written with an owner; a missing one means a cut-off line
  }
  return put(make(postId, seq, dwellMs, bytes, owner));
}

bool Index::put(const Entry &entry) {
  for (int i = 0; i < used; i++) {
    if (entries[i].postId == entry.postId) {
      entries[i] = entry;
      return true;
    }
  }
  if (used >= limit) {
    return false;
  }
  entries[used++] = entry;
  return true;
}

bool Index::remove(uint32_t postId) {
  for (int i = 0; i < used; i++) {
    if (entries[i].postId == postId) {
      entries[i] = entries[--used]; // order does not matter
      return true;
    }
  }
  return false;
}

const Entry *Index::find(uint32_t postId) const {
  for (int i = 0; i < used; i++) {
    if (entries[i].postId == postId) {
      return &entries[i];
    }
  }
  return nullptr;
}

int Index::oldest() const {
  int best = -1;
  for (int i = 0; i < used; i++) {
    if (best < 0 || entries[i].seq < entries[best].seq) {
      best = i;
    }
  }
  return best;
}

uint32_t Index::nextSeq() const {
  uint32_t highest = 0;
  for (int i = 0; i < used; i++) {
    highest = entries[i].seq > highest ? entries[i].seq : highest;
  }
  return highest + 1;
}

uint64_t Index::totalBytes() const {
  uint64_t total = 0;
  for (int i = 0; i < used; i++) {
    total += entries[i].bytes;
  }
  return total;
}

std::string Index::addLine(const Entry &entry) {
  char line[96];
  snprintf(line, sizeof(line), "A %lu %lu %lu %lu %s\n", (unsigned long)entry.postId, (unsigned long)entry.seq,
           (unsigned long)entry.dwellMs, (unsigned long)entry.bytes, entry.owner);
  return line;
}

std::string Index::removeLine(uint32_t postId) {
  char line[24];
  snprintf(line, sizeof(line), "R %lu\n", (unsigned long)postId);
  return line;
}

Entry Index::make(uint32_t postId, uint32_t seq, uint32_t dwellMs, uint32_t bytes, const char *owner) {
  Entry entry = {};
  entry.postId = postId;
  entry.seq = seq;
  entry.dwellMs = dwellMs;
  entry.bytes = bytes;
  // One word of printable ASCII, so the log line stays parseable whatever the name.
  int length = 0;
  for (const char *p = owner ? owner : ""; *p && length < OWNER_CHARS - 1; p++) {
    char c = *p;
    entry.owner[length++] = (c > ' ' && c < 127) ? c : '_';
  }
  if (length == 0) {
    entry.owner[length++] = '-';
  }
  entry.owner[length] = '\0';
  return entry;
}

} // namespace cache
