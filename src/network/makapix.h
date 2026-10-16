#pragma once

#include <Arduino.h>
#include <vector>

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
enum class QueryStatus { Ok, Unauthorized, Failed };
// Gone: Makapix refused it for good (the post was deleted, say); do not retry.
enum class SendStatus { Ok, Gone, RateLimited, Unauthorized, Failed };

// Loads the stored player key and token from NVS.
void begin();
bool isRegistered();

// Requests a new player key and the registration code to show the user.
bool provision(String &registrationCode);

// Checks whether the user has entered the code yet; stores the token once linked.
LinkStatus pollCredentials();

// Fetches the next page of artwork that fits the panel, in random order.
QueryStatus nextPage(std::vector<Artwork> &artworks);

// Tells Makapix an artwork was shown. `isoTime` is UTC; 1970-01-01T00:00:00Z
// when the clock is not set. `requested`: asked for by the user, not picked
// by the rotation. At most one report every 5 seconds is accepted.
SendStatus reportView(uint32_t postId, const String &isoTime, bool requested);

// Adds or removes a heart on an artwork, on behalf of the panel's owner.
SendStatus setLiked(uint32_t postId, bool liked);

// Drops the stored credentials so the device registers again.
void forget();

} // namespace makapix
