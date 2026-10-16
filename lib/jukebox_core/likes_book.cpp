#include "likes_book.h"

namespace likes {

int Book::find(uint32_t postId) const {
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].postId == postId) {
      return (int)i;
    }
  }
  return -1;
}

bool Book::liked(uint32_t postId) const {
  int i = find(postId);
  return i >= 0 && entries[i].wanted;
}

void Book::set(uint32_t postId, bool wanted) {
  int i = find(postId);
  Entry entry = {postId, wanted, false};
  if (i >= 0) {
    entry.remote = entries[i].remote;
    entries.erase(entries.begin() + i);
  }
  // Nothing to remember: not liked, and Makapix was never told it was.
  if (!entry.wanted && !entry.remote) {
    return;
  }
  entries.push_back(entry); // the most recent change goes last
  trim();
}

// Over capacity, forget the oldest entries that need nothing sent. Entries
// still to send are kept, so a change is never lost.
void Book::trim() {
  for (size_t i = 0; entries.size() > (size_t)CAPACITY && i < entries.size();) {
    if (entries[i].wanted == entries[i].remote) {
      entries.erase(entries.begin() + i);
    } else {
      i++;
    }
  }
}

bool Book::nextPending(uint32_t &postId, bool &wanted) const {
  for (const Entry &entry : entries) {
    if (entry.wanted != entry.remote) {
      postId = entry.postId;
      wanted = entry.wanted;
      return true;
    }
  }
  return false;
}

void Book::confirmed(uint32_t postId, bool liked) {
  int i = find(postId);
  if (i < 0) {
    return;
  }
  entries[i].remote = liked;
  if (!entries[i].wanted && !entries[i].remote) {
    entries.erase(entries.begin() + i);
  }
}

void Book::drop(uint32_t postId) {
  int i = find(postId);
  if (i >= 0) {
    entries.erase(entries.begin() + i);
  }
}

int Book::count() const {
  int n = 0;
  for (const Entry &entry : entries) {
    n += entry.wanted ? 1 : 0;
  }
  return n;
}

int Book::pending() const {
  int n = 0;
  for (const Entry &entry : entries) {
    n += entry.wanted != entry.remote ? 1 : 0;
  }
  return n;
}

std::vector<uint32_t> Book::list() const {
  std::vector<uint32_t> ids;
  for (size_t i = entries.size(); i > 0; i--) {
    if (entries[i - 1].wanted) {
      ids.push_back(entries[i - 1].postId);
    }
  }
  return ids;
}

std::string Book::save() const {
  std::string bytes;
  for (const Entry &entry : entries) {
    for (int shift = 0; shift < 32; shift += 8) {
      bytes += (char)((entry.postId >> shift) & 0xFF);
    }
    bytes += (char)((entry.wanted ? 1 : 0) | (entry.remote ? 2 : 0));
  }
  return bytes;
}

bool Book::load(const std::string &bytes) {
  // Unsent changes are never trimmed, so a saved book may run over CAPACITY.
  if (bytes.size() % 5 != 0 || bytes.size() / 5 > (size_t)CAPACITY * 4) {
    return false;
  }
  std::vector<Entry> loaded;
  for (size_t at = 0; at < bytes.size(); at += 5) {
    uint32_t postId = 0;
    for (int b = 0; b < 4; b++) {
      postId |= (uint32_t)(uint8_t)bytes[at + b] << (8 * b);
    }
    uint8_t flags = (uint8_t)bytes[at + 4];
    if (postId == 0 || flags > 3) {
      return false;
    }
    loaded.push_back({postId, (flags & 1) != 0, (flags & 2) != 0});
  }
  entries = loaded;
  return true;
}

} // namespace likes
