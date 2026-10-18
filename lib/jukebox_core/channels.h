#pragma once

#include <string>

// Which Makapix art an artwork slot plays, kept apart from the network so it
// can be tested. A channel is written as a short string, stored with the slot
// and used in the API:
//
//   all                everything public (the default)
//   promoted           featured on Makapix
//   mine               art by the account the panel is linked to
//   artist:<name>      one artist's art
//   hashtag:<tag>      posts carrying a hashtag
//   likedby:<name>     art someone has reacted to; with your own name, your likes
//
// Names and tags may start with @ or #, which is dropped. Makapix checks that
// a name exists; here only the shape is checked.

namespace channels {

enum class Kind { All, Promoted, Mine, Artist, Hashtag, LikedBy };

struct Channel {
  Kind kind = Kind::All;
  std::string value; // the name or tag, for Artist, Hashtag and LikedBy
};

// False, with `error` saying why, for anything that is not a channel.
bool parse(const std::string &spec, Channel &channel, std::string &error);

// The canonical string: hashtags lower case, no @ or #.
std::string format(const Channel &channel);

// For people: "Everything", "Promoted", "My art", "@Fab", "#cat", "Liked by @Fab".
std::string label(const Channel &channel);

// The Makapix query_posts channel name.
const char *makapixName(Kind kind);

// A play_channel command from makapix.club ("all", "promoted", "by_user" or
// "reactions" with a user handle, "hashtag" with a tag) as one of ours. False
// for anything else, or when the handle or tag it needs is missing.
bool fromMakapix(const std::string &channelName, const std::string &userHandle, const std::string &hashtag,
                 Channel &channel);

} // namespace channels
