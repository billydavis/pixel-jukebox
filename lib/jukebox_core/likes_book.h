#pragma once

#include <stdint.h>
#include <string>
#include <vector>

// The artworks liked from this panel, kept apart from storage and the network
// so it can be tested.
//
// Each entry remembers what the user wants (liked or not) and what Makapix was
// last told. Where the two differ, the change still has to be sent; that lets
// a like made offline wait for the connection, and a like taken back before it
// was sent never be sent at all. Makapix does not say which artworks are
// already liked, so a like made on the website is unknown here until it is
// liked from the panel too.

namespace likes {

struct Entry {
  uint32_t postId;
  bool wanted; // liked, as far as the user is concerned
  bool remote; // liked, as far as Makapix was last told
};

class Book {
public:
  static const int CAPACITY = 500;

  bool liked(uint32_t postId) const;
  // The user liked or unliked an artwork.
  void set(uint32_t postId, bool wanted);

  // The oldest change still to send. False when everything is sent.
  bool nextPending(uint32_t &postId, bool &wanted) const;
  // Makapix accepted the change.
  void confirmed(uint32_t postId, bool liked);
  // Makapix refused it for good (the artwork was deleted): forget the entry.
  void drop(uint32_t postId);

  int count() const;   // artworks liked
  int pending() const; // changes waiting to be sent
  // The liked artworks, the most recent first.
  std::vector<uint32_t> list() const;

  // Five bytes per entry, for saving; load() refuses anything malformed.
  std::string save() const;
  bool load(const std::string &bytes);

private:
  int find(uint32_t postId) const;
  void trim();
  std::vector<Entry> entries; // oldest first
};

} // namespace likes
