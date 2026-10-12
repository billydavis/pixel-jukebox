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
};

enum class LinkStatus { Linked, Pending, RateLimited, Failed };
enum class QueryStatus { Ok, Unauthorized, Failed };

// Loads the stored player key and token from NVS.
void begin();
bool isRegistered();

// Requests a new player key and the registration code to show the user.
bool provision(String &registrationCode);

// Checks whether the user has entered the code yet; stores the token once linked.
LinkStatus pollCredentials();

// Fetches the next page of artwork that fits the panel, in random order.
QueryStatus nextPage(std::vector<Artwork> &artworks);

// Drops the stored credentials so the device registers again.
void forget();

} // namespace makapix
