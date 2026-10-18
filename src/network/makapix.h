#pragma once

#include <Arduino.h>
#include <vector>

#include "channels.h"

// Makapix Club player API over HTTPS. Protocol reference: makapix/docs/player/.
namespace makapix {

struct Artwork {
  uint32_t postId;
  String ownerHandle;
  String gifUrl; // plain-HTTP vault URL of the GIF rendition
  uint32_t dwellMs;
  bool fromCache = false; // played from the card, not downloaded
};

enum class LinkStatus { Linked, Pending, RateLimited, Failed };
// Refused: Makapix will not serve that channel as asked (no such user, say); see `problem`.
enum class QueryStatus { Ok, Unauthorized, Refused, Failed };

// Where a channel's paging has got to. Each channel keeps its own.
struct PageState {
  String cursor;
  uint32_t seed = 0;
};
// Gone: Makapix refused it for good (the post was deleted, say); do not retry.
enum class SendStatus { Ok, Gone, RateLimited, Unauthorized, Failed };

// Loads the stored player key and token from NVS.
void begin();
bool isRegistered();

// Requests a new player key and the registration code to show the user.
bool provision(String &registrationCode);

// Checks whether the user has entered the code yet; stores the token once linked.
LinkStatus pollCredentials();

// Fetches the next page of artwork from `channel` that fits the panel, in
// random order. An empty page with nothing left means the channel has no such
// art. On Refused, `problem` says why in Makapix's words.
QueryStatus nextPage(const channels::Channel &channel, PageState &page, std::vector<Artwork> &artworks,
                     String &problem);

// Tells Makapix an artwork was shown. `isoTime` is UTC; 1970-01-01T00:00:00Z
// when the clock is not set. `requested`: asked for by the user, not picked
// by the rotation. At most one report every 5 seconds is accepted.
// `channel` and `hashtag` name where it came from ("all" when empty).
// On RateLimited, `retryAfter` is the seconds Makapix asked to wait (0 if it
// did not say).
SendStatus reportView(uint32_t postId, const String &isoTime, bool requested, const String &channel,
                      const String &hashtag, uint32_t &retryAfter);

// Adds or removes a heart on an artwork, on behalf of the panel's owner.
// `retryAfter` as for reportView.
SendStatus setLiked(uint32_t postId, bool liked, uint32_t &retryAfter);

// Drops the stored credentials so the device registers again.
void forget();

} // namespace makapix
