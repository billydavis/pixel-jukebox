#include "channels.h"

namespace channels {

static const size_t MAX_NAME_CHARS = 32; // Makapix handles are 3 to 32 characters
static const size_t MAX_TAG_CHARS = 64;

// Characters, not bytes: names may be in any script.
static size_t utf8Length(const std::string &text) {
  size_t count = 0;
  for (unsigned char c : text) {
    count += (c & 0xC0) != 0x80 ? 1 : 0;
  }
  return count;
}

static bool hasSpaceOrControl(const std::string &text) {
  for (unsigned char c : text) {
    if (c <= ' ' || c == 0x7F || c == ':' || c == '/' || c == '"' || c == '\\') {
      return true;
    }
  }
  return false;
}

static std::string lower(std::string text) {
  for (char &c : text) {
    if (c >= 'A' && c <= 'Z') {
      c = (char)(c - 'A' + 'a');
    }
  }
  return text;
}

bool parse(const std::string &spec, Channel &channel, std::string &error) {
  Channel parsed;
  if (spec == "all" || spec.empty()) {
    parsed.kind = Kind::All;
  } else if (spec == "promoted") {
    parsed.kind = Kind::Promoted;
  } else if (spec == "mine") {
    parsed.kind = Kind::Mine;
  } else if (spec == "uploads") {
    parsed.kind = Kind::Uploads;
  } else {
    size_t colon = spec.find(':');
    std::string type = spec.substr(0, colon);
    std::string value = colon == std::string::npos ? "" : spec.substr(colon + 1);
    char prefix;
    if (type == "artist") {
      parsed.kind = Kind::Artist;
      prefix = '@';
    } else if (type == "likedby") {
      parsed.kind = Kind::LikedBy;
      prefix = '@';
    } else if (type == "hashtag") {
      parsed.kind = Kind::Hashtag;
      prefix = '#';
    } else {
      error = "unknown channel \"" + spec + "\"";
      return false;
    }
    if (!value.empty() && value[0] == prefix) {
      value.erase(0, 1);
    }
    size_t length = utf8Length(value);
    size_t limit = parsed.kind == Kind::Hashtag ? MAX_TAG_CHARS : MAX_NAME_CHARS;
    if (length == 0) {
      error = parsed.kind == Kind::Hashtag ? "the hashtag is missing" : "the Makapix name is missing";
      return false;
    }
    if (length > limit || hasSpaceOrControl(value)) {
      error = parsed.kind == Kind::Hashtag ? "that is not a hashtag" : "that is not a Makapix name";
      return false;
    }
    parsed.value = parsed.kind == Kind::Hashtag ? lower(value) : value;
  }
  channel = parsed;
  return true;
}

std::string format(const Channel &channel) {
  switch (channel.kind) {
    case Kind::Promoted:
      return "promoted";
    case Kind::Mine:
      return "mine";
    case Kind::Uploads:
      return "uploads";
    case Kind::Artist:
      return "artist:" + channel.value;
    case Kind::Hashtag:
      return "hashtag:" + channel.value;
    case Kind::LikedBy:
      return "likedby:" + channel.value;
    default:
      return "all";
  }
}

std::string label(const Channel &channel) {
  switch (channel.kind) {
    case Kind::Promoted:
      return "Promoted";
    case Kind::Mine:
      return "My art";
    case Kind::Uploads:
      return "My uploads";
    case Kind::Artist:
      return "@" + channel.value;
    case Kind::Hashtag:
      return "#" + channel.value;
    case Kind::LikedBy:
      return "Liked by @" + channel.value;
    default:
      return "Everything";
  }
}

const char *makapixName(Kind kind) {
  switch (kind) {
    case Kind::Promoted:
      return "promoted";
    case Kind::Mine:
      return "user";
    case Kind::Artist:
      return "by_user";
    case Kind::Hashtag:
      return "hashtag";
    case Kind::LikedBy:
      return "reactions";
    default:
      return "all";
  }
}

bool fromMakapix(const std::string &channelName, const std::string &userHandle, const std::string &hashtag,
                 Channel &channel) {
  std::string spec;
  if (channelName == "all" || channelName == "promoted") {
    spec = channelName;
  } else if (channelName == "by_user" && !userHandle.empty()) {
    spec = "artist:" + userHandle;
  } else if (channelName == "reactions" && !userHandle.empty()) {
    spec = "likedby:" + userHandle;
  } else if (channelName == "hashtag" && !hashtag.empty()) {
    spec = "hashtag:" + hashtag;
  } else {
    return false;
  }
  std::string error;
  return parse(spec, channel, error);
}

} // namespace channels
