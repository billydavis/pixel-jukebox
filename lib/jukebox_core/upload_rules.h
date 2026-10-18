#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

// Rules for art uploaded from a phone or PC (storage/uploads.cpp), kept apart
// from the card so they can be tested.
//
// Each upload is one file in the uploads folder, named with everything the
// list needs, so start-up reads the folder once and opens nothing:
//
//   12-32x32-cat on a mat.gif     id 12, 32 by 32, called "cat on a mat"

namespace uploads {

static const int MAX_SIDE = 64;               // the panel
static const size_t MAX_BYTES = 512 * 1024;   // the biggest Makapix art seen so far is about 350 KB
static const int MAX_UPLOADS = 200;
static const size_t MAX_NAME = 40;

// The canvas size from a GIF's header. False for anything that is not a GIF
// (GIF87a or GIF89a), or that is too short to have a header.
bool gifSize(const uint8_t *data, size_t length, int &width, int &height);

// Why a GIF of this size cannot be uploaded, or "" when it can.
std::string sizeProblem(int width, int height, size_t bytes);

// A name made safe for a file and for the page: letters, digits, spaces,
// dots, dashes and underscores kept, anything else becomes "_", a trailing
// ".gif" dropped, at most MAX_NAME characters, "upload" when nothing is left.
std::string tidyName(const std::string &name);

std::string fileName(uint32_t id, int width, int height, const std::string &name);
// False for anything in the folder that is not an upload's file.
bool parseFileName(const std::string &file, uint32_t &id, int &width, int &height, std::string &name);

// Undoes the percent-encoding a browser puts on a file name sent in a header
// (encodeURIComponent). A malformed escape is kept as it is.
std::string percentDecode(const std::string &text);

} // namespace uploads
