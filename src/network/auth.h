#pragma once

#include <Arduino.h>

// The optional PIN for the control page and the API. With no PIN set every
// request is let in, as before. With one set, a request needs a signed-in
// browser's session (the cookie from POST /auth/login, or the token in an
// "Authorization: Bearer" header) or the PIN itself in an X-Jukebox-Pin header,
// for scripts. Five wrong PINs refuse every PIN for 30 seconds.
//
// The PIN is kept as a salted SHA-256, never as itself. Sessions are kept in
// flash too, so a restart does not sign every browser out. Forgot the PIN?
// Hold the panel's button for 5 seconds and tick "Remove the PIN" on the
// Wi-Fi setup page.

#define AUTH_COOKIE "jukebox_session"
#define AUTH_PIN_HEADER "X-Jukebox-Pin"

enum class AuthResult {
  Allowed,
  Missing, // no session or PIN: 401
  Wrong,   // a PIN that is wrong: 401
  Locked,  // too many wrong PINs: 429
};

void authBegin();

bool authPinSet();
// Seconds before PINs are accepted again; 0 when not locked.
int authLockedSeconds();
int authSessionCount();

// Checks a request's credentials. Any of them may be empty.
AuthResult authCheck(const String &cookieHeader, const String &authorization, const String &pin);

// Signs a browser in with the PIN. On success `token` is its new session
// (empty when no PIN is set, as there is nothing to sign in to).
AuthResult authLogin(const String &pin, String &token);
void authLogout(const String &cookieHeader, const String &authorization);

// Sets, changes or (with an empty `pin`) removes the PIN. With a PIN set,
// `current` must be it. Every browser is signed out. False with `error`, and
// `result` saying whether it was the current PIN, otherwise.
bool authSetPin(const String &pin, const String &current, AuthResult &result, String &error);

// Removes the PIN without asking for it: only for the Wi-Fi setup page, when
// someone has proved they are at the panel by holding its button.
void authRemovePin();
