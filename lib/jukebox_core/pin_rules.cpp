#include "pin_rules.h"

namespace pin {

bool valid(const std::string &pin) {
  if (pin.size() < 4 || pin.size() > 8) {
    return false;
  }
  for (char c : pin) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

int Lockout::lockedSeconds(uint32_t now) const {
  if (!locked_) {
    return 0;
  }
  uint32_t elapsed = now - lockedAt_;
  if (elapsed >= LOCK_MS) {
    return 0;
  }
  return (int)((LOCK_MS - elapsed + 999) / 1000);
}

bool Lockout::attempt(bool right, uint32_t now, bool *justLocked) {
  if (justLocked) {
    *justLocked = false;
  }
  if (lockedSeconds(now) > 0) {
    return false; // not counted either way
  }
  if (locked_) {
    locked_ = false; // the lock ran out: a fresh start
    failures_ = 0;
  }
  if (right) {
    failures_ = 0;
    return true;
  }
  if (++failures_ >= MAX_FAILURES) {
    locked_ = true;
    lockedAt_ = now;
    failures_ = 0;
    if (justLocked) {
      *justLocked = true;
    }
  }
  return false;
}

static bool wellFormed(const std::string &token) {
  if (token.size() != (size_t)Sessions::TOKEN_CHARS) {
    return false;
  }
  for (char c : token) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

std::string Sessions::create(const uint8_t random[16], uint32_t now) {
  static const char HEX[] = "0123456789abcdef";
  std::string token;
  for (int i = 0; i < 16; i++) {
    token += HEX[random[i] >> 4];
    token += HEX[random[i] & 0x0f];
  }
  // A free slot, or else the one unseen for longest.
  Slot *pick = &slots_[0];
  for (Slot &slot : slots_) {
    if (slot.token.empty()) {
      pick = &slot;
      break;
    }
    if (now - slot.lastSeen > now - pick->lastSeen) {
      pick = &slot;
    }
  }
  pick->token = token;
  pick->lastSeen = now;
  return token;
}

bool Sessions::touch(const std::string &token, uint32_t now) {
  if (!wellFormed(token)) {
    return false;
  }
  for (Slot &slot : slots_) {
    if (slot.token == token) {
      slot.lastSeen = now;
      return true;
    }
  }
  return false;
}

void Sessions::end(const std::string &token) {
  if (token.empty()) {
    return;
  }
  for (Slot &slot : slots_) {
    if (slot.token == token) {
      slot.token.clear();
    }
  }
}

void Sessions::endAll() {
  for (Slot &slot : slots_) {
    slot.token.clear();
  }
}

int Sessions::count() const {
  int n = 0;
  for (const Slot &slot : slots_) {
    n += slot.token.empty() ? 0 : 1;
  }
  return n;
}

std::vector<std::string> Sessions::tokens() const {
  std::vector<std::string> list;
  for (const Slot &slot : slots_) {
    if (!slot.token.empty()) {
      list.push_back(slot.token);
    }
  }
  return list;
}

void Sessions::restore(const std::vector<std::string> &tokens, uint32_t now) {
  endAll();
  int next = 0;
  for (const std::string &token : tokens) {
    if (next < MAX && wellFormed(token)) {
      slots_[next].token = token;
      slots_[next].lastSeen = now;
      next++;
    }
  }
}

std::string cookieValue(const std::string &header, const std::string &name) {
  size_t pos = 0;
  while (pos < header.size()) {
    while (pos < header.size() && (header[pos] == ' ' || header[pos] == ';')) {
      pos++;
    }
    size_t end = header.find(';', pos);
    std::string pair = header.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    size_t equals = pair.find('=');
    if (equals != std::string::npos && pair.compare(0, equals, name) == 0 && equals == name.size()) {
      return pair.substr(equals + 1);
    }
    if (end == std::string::npos) {
      break;
    }
    pos = end + 1;
  }
  return "";
}

bool sameBytes(const uint8_t *a, const uint8_t *b, size_t length) {
  uint8_t difference = 0;
  for (size_t i = 0; i < length; i++) {
    difference |= a[i] ^ b[i];
  }
  return difference == 0;
}

} // namespace pin
