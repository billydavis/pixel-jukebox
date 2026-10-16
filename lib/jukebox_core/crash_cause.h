#pragma once

#include <stdint.h>

// Names for the Xtensa exception causes (EXCCAUSE) a crash record holds, as
// the ESP-IDF panic handler prints them, so the API can say "LoadProhibited"
// rather than "28".

namespace crash {

// The name for `cause`, or "Unknown" for one the CPU does not define.
const char *causeName(uint32_t cause);

// A plain-words hint for the common causes, or nullptr.
const char *causeHint(uint32_t cause);

} // namespace crash
