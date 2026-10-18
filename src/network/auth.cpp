#include "network/auth.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>

#include "pin_rules.h"

static const size_t SALT_BYTES = 16;
static const size_t HASH_BYTES = 32;

static Preferences prefs;
static SemaphoreHandle_t lock;
static bool pinSet = false;
static uint8_t salt[SALT_BYTES];
static uint8_t hash[HASH_BYTES];
static pin::Lockout lockout;
static pin::Sessions sessions;

namespace {
struct Guard {
  Guard() { xSemaphoreTakeRecursive(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGiveRecursive(lock); }
};
} // namespace

static void hashPin(const String &pin, uint8_t out[HASH_BYTES]) {
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  mbedtls_sha256_starts_ret(&context, 0);
  mbedtls_sha256_update_ret(&context, salt, SALT_BYTES);
  mbedtls_sha256_update_ret(&context, (const uint8_t *)pin.c_str(), pin.length());
  mbedtls_sha256_finish_ret(&context, out);
  mbedtls_sha256_free(&context);
}

// One string of tokens separated by spaces; written only when a browser signs
// in or out, never on every request.
static void saveSessions() {
  String joined;
  for (const std::string &token : sessions.tokens()) {
    joined += token.c_str();
    joined += ' ';
  }
  prefs.putString("sessions", joined);
}

static void loadSessions() {
  std::vector<std::string> tokens;
  String joined = prefs.getString("sessions", "");
  int start = 0;
  while (start < (int)joined.length()) {
    int space = joined.indexOf(' ', start);
    if (space < 0) {
      space = joined.length();
    }
    if (space > start) {
      tokens.push_back(joined.substring(start, space).c_str());
    }
    start = space + 1;
  }
  sessions.restore(tokens, millis());
}

void authBegin() {
  lock = xSemaphoreCreateRecursiveMutex();
  prefs.begin("auth", false);
  pinSet = prefs.getBytesLength("salt") == SALT_BYTES && prefs.getBytesLength("hash") == HASH_BYTES &&
           prefs.getBytes("salt", salt, SALT_BYTES) == SALT_BYTES &&
           prefs.getBytes("hash", hash, HASH_BYTES) == HASH_BYTES;
  if (pinSet) {
    loadSessions();
  }
  Serial.printf("PIN: %s\n", pinSet ? "set, the API needs a session" : "not set");
}

bool authPinSet() {
  Guard guard;
  return pinSet;
}

int authLockedSeconds() {
  Guard guard;
  return lockout.lockedSeconds(millis());
}

int authSessionCount() {
  Guard guard;
  return sessions.count();
}

// Counts towards the lockout.
static AuthResult checkPin(const String &pin) {
  if (lockout.lockedSeconds(millis()) > 0) {
    return AuthResult::Locked;
  }
  uint8_t attempt[HASH_BYTES];
  hashPin(pin, attempt);
  bool justLocked = false;
  if (lockout.attempt(pin::sameBytes(attempt, hash, HASH_BYTES), millis(), &justLocked)) {
    return AuthResult::Allowed;
  }
  if (justLocked) {
    Serial.printf("PIN: %d wrong in a row, refusing PINs for %lu s\n", pin::Lockout::MAX_FAILURES,
                  (unsigned long)(pin::Lockout::LOCK_MS / 1000));
    return AuthResult::Locked;
  }
  return AuthResult::Wrong;
}

static std::string sessionToken(const String &cookieHeader, const String &authorization) {
  std::string token = pin::cookieValue(cookieHeader.c_str(), AUTH_COOKIE);
  if (token.empty() && authorization.startsWith("Bearer ")) {
    token = authorization.substring(7).c_str();
  }
  return token;
}

AuthResult authCheck(const String &cookieHeader, const String &authorization, const String &pin) {
  Guard guard;
  if (!pinSet) {
    return AuthResult::Allowed;
  }
  std::string token = sessionToken(cookieHeader, authorization);
  if (!token.empty() && sessions.touch(token, millis())) {
    return AuthResult::Allowed;
  }
  if (!pin.isEmpty()) {
    return checkPin(pin);
  }
  return lockout.lockedSeconds(millis()) > 0 ? AuthResult::Locked : AuthResult::Missing;
}

AuthResult authLogin(const String &pin, String &token) {
  Guard guard;
  token = "";
  if (!pinSet) {
    return AuthResult::Allowed;
  }
  AuthResult result = checkPin(pin);
  if (result != AuthResult::Allowed) {
    return result;
  }
  uint8_t random[16];
  esp_fill_random(random, sizeof(random));
  token = sessions.create(random, millis()).c_str();
  saveSessions();
  return AuthResult::Allowed;
}

void authLogout(const String &cookieHeader, const String &authorization) {
  Guard guard;
  std::string token = sessionToken(cookieHeader, authorization);
  int before = sessions.count();
  sessions.end(token);
  if (sessions.count() != before) {
    saveSessions();
  }
}

bool authSetPin(const String &newPin, const String &current, AuthResult &result, String &error) {
  Guard guard;
  result = AuthResult::Allowed;
  if (!newPin.isEmpty() && !pin::valid(newPin.c_str())) {
    error = "pin must be 4 to 8 digits";
    return false;
  }
  if (pinSet) {
    result = checkPin(current);
    if (result != AuthResult::Allowed) {
      error = result == AuthResult::Locked ? "Too many wrong PINs, wait and try again" : "current is not the PIN";
      return false;
    }
  }
  sessions.endAll();
  if (newPin.isEmpty()) {
    prefs.clear();
    pinSet = false;
    Serial.println("PIN: removed");
    return true;
  }
  esp_fill_random(salt, SALT_BYTES);
  hashPin(newPin, hash);
  bool saved = prefs.putBytes("salt", salt, SALT_BYTES) == SALT_BYTES &&
               prefs.putBytes("hash", hash, HASH_BYTES) == HASH_BYTES;
  prefs.remove("sessions");
  if (!saved) {
    prefs.clear();
    pinSet = false;
    error = "Could not save the PIN";
    return false;
  }
  pinSet = true;
  Serial.println("PIN: set");
  return true;
}

void authRemovePin() {
  Guard guard;
  prefs.clear();
  sessions.endAll();
  pinSet = false;
  Serial.println("PIN: removed from the Wi-Fi setup page");
}
