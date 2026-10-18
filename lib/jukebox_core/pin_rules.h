#pragma once

#include <stdint.h>
#include <string>
#include <vector>

// The control-page PIN's rules, kept apart from the web server and the flash so
// they can be tested: what a valid PIN is, the lockout after wrong guesses, the
// table of signed-in browsers, and reading a cookie. The design follows p64's
// (github.com/fabkury/p64, firmware/components/p64_web/src/auth.cpp); the code
// here is our own. Times are milliseconds from millis(), which wraps after 49
// days, so they are only ever compared by subtraction.

namespace pin {

// 4 to 8 digits.
bool valid(const std::string &pin);

// Five wrong PINs in a row refuse every PIN, right or wrong, for 30 seconds.
// A right one resets the count. Browsers already signed in are not affected.
class Lockout {
public:
  static const int MAX_FAILURES = 5;
  static const uint32_t LOCK_MS = 30000;

  // Seconds left, rounded up; 0 when open.
  int lockedSeconds(uint32_t now) const;
  // Records an attempt and returns whether it gets in. `justLocked` is set
  // when this attempt started a lockout.
  bool attempt(bool right, uint32_t now, bool *justLocked = nullptr);

private:
  int failures_ = 0;
  bool locked_ = false;
  uint32_t lockedAt_ = 0;
};

// Up to eight signed-in browsers, each known by a random 32-hex-digit token.
// A ninth takes the place of the one seen longest ago.
class Sessions {
public:
  static const int MAX = 8;
  static const int TOKEN_CHARS = 32;

  // A new session from 16 random bytes; returns its token.
  std::string create(const uint8_t random[16], uint32_t now);
  // True, and the session marked as just seen, when `token` is signed in.
  bool touch(const std::string &token, uint32_t now);
  void end(const std::string &token);
  void endAll();
  int count() const;

  // The tokens, to keep them in flash so a restart does not sign everyone out,
  // and back. restore() skips anything that is not a well-formed token.
  std::vector<std::string> tokens() const;
  void restore(const std::vector<std::string> &tokens, uint32_t now);

private:
  struct Slot {
    std::string token; // empty when free
    uint32_t lastSeen = 0;
  };
  Slot slots_[MAX];
};

// The value of cookie `name` in a Cookie header ("a=1; b=2"), or "".
std::string cookieValue(const std::string &header, const std::string &name);

// Compares two byte strings in a time that does not depend on where they differ.
bool sameBytes(const uint8_t *a, const uint8_t *b, size_t length);

} // namespace pin
