#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

// The index of artworks cached on the card, kept apart from the card itself so
// it can be tested. It is stored as a log of one-line records, appended as
// things change and replayed at start-up, which avoids rewriting a large file
// every time an artwork arrives:
//
//   A <postId> <seq> <dwellMs> <bytes> <owner> [<key>]   an artwork was stored or played
//   R <postId>                                             an artwork was removed
//
// <key> is the post's Makapix storage key (a UUID), which gives its page on
// makapix.club. Lines written before it was kept have none.
//
// `seq` grows with every store or play, so the lowest is the artwork that has
// gone longest unplayed. It needs no clock, which matters because the board
// only knows the time once it has been online.

namespace cache {

static const int OWNER_CHARS = 28; // including the terminating zero
static const int KEY_CHARS = 37;   // a UUID and the terminating zero

struct Entry {
  uint32_t postId;
  uint32_t seq;
  uint32_t dwellMs;
  uint32_t bytes;
  char owner[OWNER_CHARS];
  char key[KEY_CHARS]; // empty when not known
};

class Index {
 public:
  // Uses `storage` for up to `capacity` entries; the caller owns the memory.
  Index(Entry *storage, int capacity);

  void clear();

  // Replays one log line. Returns false, and changes nothing, for a line that
  // is malformed, such as one cut short by a power loss.
  bool apply(const char *line);

  // Adds an entry, or replaces the one with the same postId. False when full.
  bool put(const Entry &entry);
  bool remove(uint32_t postId);

  const Entry *find(uint32_t postId) const;
  int count() const { return used; }
  int capacity() const { return limit; }
  const Entry &at(int index) const { return entries[index]; }

  // The entry that has gone longest without being stored or played, or -1.
  int oldest() const;
  uint32_t nextSeq() const;
  uint64_t totalBytes() const;

  static std::string addLine(const Entry &entry);
  static std::string removeLine(uint32_t postId);

  // Fills `entry` with the fields given, keeping the owner to one safe word.
  // A key that is not made of hex digits and dashes is left out.
  static Entry make(uint32_t postId, uint32_t seq, uint32_t dwellMs, uint32_t bytes, const char *owner,
                    const char *key = "");

 private:
  Entry *entries;
  int limit;
  int used = 0;
};

} // namespace cache
