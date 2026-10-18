#include "upload_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace uploads {

bool gifSize(const uint8_t *data, size_t length, int &width, int &height) {
  if (length < 10 || (memcmp(data, "GIF87a", 6) != 0 && memcmp(data, "GIF89a", 6) != 0)) {
    return false;
  }
  width = data[6] | (data[7] << 8);
  height = data[8] | (data[9] << 8);
  return true;
}

std::string sizeProblem(int width, int height, size_t bytes) {
  if (width < 1 || height < 1) {
    return "the GIF has no size";
  }
  if (width > MAX_SIDE || height > MAX_SIDE) {
    return "it is " + std::to_string(width) + " x " + std::to_string(height) +
           "; the panel is 64 x 64, so art must be 64 x 64 or smaller";
  }
  if (bytes > MAX_BYTES) {
    return "it is over 512 KB";
  }
  return "";
}

std::string tidyName(const std::string &name) {
  std::string text = name;
  if (text.size() >= 4) {
    std::string ending = text.substr(text.size() - 4);
    for (char &c : ending) {
      c = (char)tolower((unsigned char)c);
    }
    if (ending == ".gif") {
      text.resize(text.size() - 4);
    }
  }
  std::string tidy;
  for (char c : text) {
    bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
                c == '.' || c == '-' || c == '_';
    tidy += keep ? c : '_';
    if (tidy.size() == MAX_NAME) {
      break;
    }
  }
  // No spaces or dots at the ends: FAT trims them, and they look odd.
  size_t start = tidy.find_first_not_of(" .");
  size_t end = tidy.find_last_not_of(" .");
  tidy = start == std::string::npos ? "" : tidy.substr(start, end - start + 1);
  return tidy.empty() ? "upload" : tidy;
}

std::string fileName(uint32_t id, int width, int height, const std::string &name) {
  char prefix[32];
  snprintf(prefix, sizeof(prefix), "%u-%dx%d-", (unsigned)id, width, height);
  return prefix + tidyName(name) + ".gif";
}

bool parseFileName(const std::string &file, uint32_t &id, int &width, int &height, std::string &name) {
  if (file.size() < 10 || file.compare(file.size() - 4, 4, ".gif") != 0) {
    return false;
  }
  const char *text = file.c_str();
  char *end = nullptr;
  unsigned long parsedId = strtoul(text, &end, 10);
  if (end == text || *end != '-' || parsedId == 0) {
    return false;
  }
  const char *dims = end + 1;
  long w = strtol(dims, &end, 10);
  if (end == dims || *end != 'x') {
    return false;
  }
  const char *heightText = end + 1;
  long h = strtol(heightText, &end, 10);
  if (end == heightText || *end != '-' || w < 1 || h < 1 || w > MAX_SIDE || h > MAX_SIDE) {
    return false;
  }
  const char *nameStart = end + 1;
  size_t nameLength = file.size() - 4 - (nameStart - text);
  if (nameLength == 0) {
    return false;
  }
  id = (uint32_t)parsedId;
  width = (int)w;
  height = (int)h;
  name.assign(nameStart, nameLength);
  return true;
}

static int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

std::string percentDecode(const std::string &text) {
  std::string out;
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] == '%' && i + 2 < text.size()) {
      int high = hexValue(text[i + 1]), low = hexValue(text[i + 2]);
      if (high >= 0 && low >= 0) {
        out += (char)(high * 16 + low);
        i += 2;
        continue;
      }
    }
    out += text[i];
  }
  return out;
}

} // namespace uploads
