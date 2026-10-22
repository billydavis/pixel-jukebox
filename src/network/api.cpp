#include "network/api.h"

#include <ArduinoJson.h>
#include <functional>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <uri/UriBraces.h>

#include "app/clock_chip.h"
#include "app/commands.h"
#include "app/crash_report.h"
#include "app/firmware_update.h"
#include "config.h"
#include "display/display.h"
#include "input/case_climate.h"
#include "app/memory_trace.h"
#include "app/mode.h"
#include "app/night.h"
#include "app/rotation.h"
#include "app/timezone.h"
#include "timezones.h"
#include "channels.h"
#include "test_patterns.h"
#include "app/status.h"
#include "app/telemetry.h"
#include "app/transition.h"
#include "network/auth.h"
#include "network/live.h"
#include "network/makapix.h"
#include "network/reporter.h"
#include "network/web_assets.h"
#include "slots/slot_makapix.h"
#include "storage/art_cache.h"
#include "storage/card.h"
#include "network/weather.h"
#include "slots/slot_clock.h"
#include "storage/starter_art.h"
#include "storage/uploads.h"
#include "upload_rules.h"
#include "transitions.h"

#define API_BASE "/api/v1"

static const uint32_t SERVER_STACK_BYTES = 8192;
static const BaseType_t SERVER_CORE = 0; // away from the display loop on core 1
static const uint32_t SERVER_POLL_MS = 2;

static WebServer server(80);
static bool started = false;

// The slowest request since start-up, for /diagnostics. A request that waits
// holds up every other, since the server takes one at a time.
static uint32_t slowestMs = 0, slowCount = 0;
static String slowestUri;
// Inside a handler: the whole of it, and for card pictures the read and the
// send apart, to tell a slow card from a slow network.
static uint32_t slowestHandlerMs = 0, slowestReadMs = 0, slowestSendMs = 0;
static String slowestHandlerUri;

static void sendJson(int status, const JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  server.send(status, "application/json", body);
}

static void sendError(int status, const char *message) {
  JsonDocument doc;
  doc["error"] = message;
  sendJson(status, doc);
}

// Commands are carried out by the display loop, so the reply only confirms
// that one was queued. GET /status shows the result.
static void sendQueued(bool queued, const char *command) {
  if (!queued) {
    sendError(503, "Command queue is full, try again");
    return;
  }
  JsonDocument doc;
  doc["queued"] = command;
  sendJson(202, doc);
}

// Parses the request body as a JSON object. An empty body gives an empty object.
static bool readBody(JsonDocument &doc) {
  String body = server.arg("plain");
  if (body.isEmpty()) {
    doc.to<JsonObject>();
    return true;
  }
  if (deserializeJson(doc, body) || !doc.is<JsonObject>()) {
    sendError(400, "Body must be a JSON object");
    return false;
  }
  return true;
}

// ---- the PIN -------------------------------------------------------------------

static void sendAuthRefusal(AuthResult result) {
  if (result == AuthResult::Locked) {
    int seconds = authLockedSeconds();
    server.sendHeader("Retry-After", String(seconds > 0 ? seconds : 1));
    sendError(429, "Too many wrong PINs, wait and try again");
  } else if (result == AuthResult::Wrong) {
    sendError(401, "Wrong PIN");
  } else {
    sendError(401, "PIN required");
  }
}

static AuthResult requestAuth() {
  return authCheck(server.header("Cookie"), server.header("Authorization"), server.header(AUTH_PIN_HEADER));
}

// Every route but the page itself, the API root and sign-in goes through this.
static WebServer::THandlerFunction guarded(WebServer::THandlerFunction handler) {
  return [handler]() {
    AuthResult result = requestAuth();
    if (result != AuthResult::Allowed) {
      sendAuthRefusal(result);
      return;
    }
    uint32_t startedAt = millis();
    handler();
    uint32_t took = millis() - startedAt;
    if (took > slowestHandlerMs) {
      slowestHandlerMs = took;
      slowestHandlerUri = server.uri();
    }
  };
}

static void sendAuth(int status, bool signedIn) {
  JsonDocument doc;
  doc["pinSet"] = authPinSet();
  doc["signedIn"] = signedIn;
  doc["lockedSeconds"] = authLockedSeconds();
  sendJson(status, doc);
}

static void handleGetAuth() {
  // Asking does not count as a guess, so a PIN header is ignored here.
  sendAuth(200, authCheck(server.header("Cookie"), server.header("Authorization"), "") == AuthResult::Allowed);
}

static void handleLogin() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["pin"].is<const char *>()) {
    sendError(400, "pin must be a string of digits");
    return;
  }
  String token;
  AuthResult result = authLogin(body["pin"].as<String>(), token);
  if (result != AuthResult::Allowed) {
    sendAuthRefusal(result);
    return;
  }
  if (!token.isEmpty()) {
    // A year, so a phone stays signed in; HttpOnly keeps the page's script off it.
    server.sendHeader("Set-Cookie",
                      AUTH_COOKIE "=" + token + "; Path=/; Max-Age=31536000; SameSite=Strict; HttpOnly");
  }
  sendAuth(200, true);
}

static void handleLogout() {
  authLogout(server.header("Cookie"), server.header("Authorization"));
  server.sendHeader("Set-Cookie", AUTH_COOKIE "=; Path=/; Max-Age=0; SameSite=Strict; HttpOnly");
  sendAuth(200, !authPinSet());
}

// {"pin": "1234", "current": "0000"}; "pin": "" removes it. Every browser is
// signed out, this one too: the page signs in again with the new PIN.
static void handlePutPin() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["pin"].is<const char *>()) {
    sendError(400, "pin must be a string: 4 to 8 digits, or \"\" to remove it");
    return;
  }
  if (!body["current"].isNull() && !body["current"].is<const char *>()) {
    sendError(400, "current must be a string");
    return;
  }
  AuthResult result;
  String error;
  if (!authSetPin(body["pin"].as<String>(), body["current"] | "", result, error)) {
    if (result == AuthResult::Locked) {
      sendAuthRefusal(result);
    } else {
      sendError(result == AuthResult::Allowed ? 400 : 403, error.c_str());
    }
    return;
  }
  sendAuth(200, !authPinSet());
}

static void handlePage() { server.send_P(200, "text/html", WEB_INDEX_HTML); }

// The gallery became the control page's Art tab; old links still land there.
static void handleGalleryPage() {
  server.sendHeader("Location", "/#art");
  server.send(302, "text/plain", "");
}

static void handleApiRoot() {
  JsonDocument doc;
  doc["name"] = "Pixel Jukebox";
  doc["api"] = API_BASE;
  sendJson(200, doc);
}

static void handleStatus() {
  PlayerStatus status = statusGet();

  JsonDocument doc;
  JsonObject device = doc["device"].to<JsonObject>();
  device["name"] = MDNS_HOSTNAME;
  device["model"] = DEVICE_MODEL;
  device["firmware"] = FIRMWARE_VERSION;
  device["ip"] = WiFi.localIP().toString();
  device["ssid"] = WiFi.SSID();
  device["rssi"] = WiFi.RSSI();
  device["uptimeSeconds"] = millis() / 1000;

  JsonObject slot = doc["slot"].to<JsonObject>();
  if (status.slot.isEmpty()) {
    slot["name"] = nullptr;
  } else {
    slot["name"] = status.slot;
  }
  slot["held"] = status.held;

  if (status.hasArtwork && status.starterId) {
    // Starter art: built in, not a Makapix post.
    JsonObject artwork = doc["artwork"].to<JsonObject>();
    artwork["postId"] = nullptr;
    artwork["source"] = "starter";
    JsonObject starter = artwork["starter"].to<JsonObject>();
    starter["id"] = status.starterId;
    starter["title"] = status.starterTitle;
    starter["credit"] = status.starterCredit;
    if (status.channel.isEmpty()) {
      artwork["channel"] = nullptr; // shown for want of Makapix art
    } else {
      artwork["channel"] = status.channel;
    }
  } else if (status.hasArtwork && status.uploadId) {
    // An upload: not a Makapix post, so no owner, link or like.
    JsonObject artwork = doc["artwork"].to<JsonObject>();
    artwork["postId"] = nullptr;
    artwork["source"] = "upload";
    JsonObject upload = artwork["upload"].to<JsonObject>();
    upload["id"] = status.uploadId;
    upload["name"] = status.uploadName;
    artwork["channel"] = status.channel;
  } else if (status.hasArtwork) {
    JsonObject artwork = doc["artwork"].to<JsonObject>();
    artwork["postId"] = status.postId;
    artwork["owner"] = status.owner;
    artwork["gifUrl"] = status.gifUrl;
    if (status.storageKey.isEmpty()) {
      artwork["pageUrl"] = nullptr; // saved to the card before the page was known
    } else {
      artwork["pageUrl"] = String(MAKAPIX_POST_PAGE_URL) + status.storageKey;
    }
    artwork["source"] = status.fromCache ? "card" : "makapix";
    artwork["liked"] = reporterLiked(status.postId);
    if (status.channel.isEmpty()) {
      artwork["channel"] = nullptr;
    } else {
      artwork["channel"] = status.channel;
    }
  } else {
    doc["artwork"] = nullptr;
  }

  if (status.slot == "weather") {
    WeatherReading now = weatherReading();
    JsonObject weather = doc["weather"].to<JsonObject>();
    weather["place"] = weatherPlace().name;
    weather["units"] = weatherFahrenheit() ? "f" : "c";
    if (now.valid) {
      weather["temperature"] = weatherFahrenheit() ? now.tempF : now.tempC;
      weather["condition"] = now.text;
    } else {
      weather["temperature"] = nullptr;
      weather["condition"] = nullptr;
    }
  }
  doc["screen"] = status.screen.isEmpty() ? "rotation" : status.screen;
  doc["mode"] = modeIsSound() ? "sound" : "rotation";
  doc["visualizer"] = modeVisualizer();
  String overrideFrom;
  String override = rotationChannelOverride(&overrideFrom);
  if (override.isEmpty()) {
    doc["channelOverride"] = nullptr;
  } else {
    JsonObject channel = doc["channelOverride"].to<JsonObject>();
    channel["channel"] = override;
    channel["from"] = overrideFrom;
  }
  NightNow night = nightNow();
  doc["brightness"] = night.brightness;
  JsonObject nightState = doc["night"].to<JsonObject>();
  nightState["active"] = night.night;
  nightState["panel"] = night.off ? "off" : night.clockOnly ? "clock" : "playing";
  nightState["overridden"] = night.overridden;
  JsonObject panel = doc["panel"].to<JsonObject>();
  panel["photo"] = displayMode() == PanelMode::Photo;
  panel["refreshHz"] = display->refreshHz();
  orientation::Orientation turned = displayWantedOrientation();
  panel["rotation"] = orientation::toDegrees(turned);
  panel["mirror"] = turned.mirror;
  sendJson(200, doc);
}

static void handleDiagnostics() {
  JsonDocument doc;
  telemetryReport(doc.to<JsonObject>());
  clockReport(doc["clock"].to<JsonObject>());
  caseClimateReport(doc["case"].to<JsonObject>());
  crashBrief(doc["crash"].to<JsonObject>());
  cardReport(doc["card"].to<JsonObject>());
  artCacheReport(doc["cache"].to<JsonObject>());
  reporterReportViews(doc["views"].to<JsonObject>());
  reporterReportLikes(doc["likes"].to<JsonObject>());
  liveReport(doc["live"].to<JsonObject>());
  JsonObject web = doc["web"].to<JsonObject>();
  web["slowestMs"] = slowestMs;
  web["slowestUri"] = slowestUri;
  web["overOneSecond"] = slowCount;
  web["slowestHandlerMs"] = slowestHandlerMs;
  web["slowestHandlerUri"] = slowestHandlerUri;
  web["slowestCardReadMs"] = slowestReadMs;
  web["slowestPictureSendMs"] = slowestSendMs;
  cardLockReport(doc["cardLock"].to<JsonObject>());
  JsonObject firmware = doc["firmware"].to<JsonObject>();
  firmware["version"] = FIRMWARE_VERSION;
  firmwareUpdateReport(firmware);
  sendJson(200, doc);
}

static void handleNext() { sendQueued(commandSend(CommandType::Next), "next"); }

// {"held": true|false} sets the state; no body toggles it.
static void handleHold() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (body["held"].isNull()) {
    sendQueued(commandSend(CommandType::ToggleHold), "toggleHold");
    return;
  }
  if (!body["held"].is<bool>()) {
    sendError(400, "held must be true or false");
    return;
  }
  bool held = body["held"];
  sendQueued(commandSend(CommandType::SetHold, held ? 1 : 0), held ? "hold" : "release");
}

static void handleGetBrightness() {
  JsonDocument doc;
  doc["value"] = nightNow().brightness; // what is showing: the night schedule may have dimmed it
  sendJson(200, doc);
}

static void handlePutBrightness() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["value"].is<int>()) {
    sendError(400, "value must be a whole number from 0 to 255");
    return;
  }
  int value = body["value"];
  if (value < 0 || value > 255) {
    sendError(400, "value must be a whole number from 0 to 255");
    return;
  }
  sendQueued(commandSend(CommandType::SetBrightness, (uint8_t)value), "brightness");
}

static void sendMode() {
  JsonDocument doc;
  doc["mode"] = modeIsSound() ? "sound" : "rotation";
  doc["visualizer"] = modeVisualizer();
  JsonArray visualizers = doc["visualizers"].to<JsonArray>();
  for (const SlotInfo &info : modeVisualizers()) {
    JsonObject item = visualizers.add<JsonObject>();
    item["name"] = info.name;
    item["label"] = info.label;
  }
  sendJson(200, doc);
}

static void handleGetMode() { sendMode(); }

// {"mode": "sound", "visualizer": "fire"} or {"mode": "rotation"}. Applied
// directly; the panel switches within a moment.
static void handlePutMode() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  String mode = body["mode"] | "";
  if (mode != "sound" && mode != "rotation") {
    sendError(400, "mode must be \"sound\" or \"rotation\"");
    return;
  }
  if (!body["visualizer"].isNull() && !body["visualizer"].is<const char *>()) {
    sendError(400, "visualizer must be a name from the visualizers list");
    return;
  }
  String error;
  if (!modeSet(mode == "sound", body["visualizer"] | "", error)) {
    sendError(400, error.c_str());
    return;
  }
  sendMode();
}

static void sendTransition() {
  JsonDocument doc;
  doc["transition"] = transitionChoice();
  JsonArray available = doc["available"].to<JsonArray>();
  for (int i = 0; i < transitions::count(); i++) {
    JsonObject item = available.add<JsonObject>();
    item["name"] = transitions::info(i).name;
    item["label"] = transitions::info(i).label;
  }
  sendJson(200, doc);
}

static void handleGetTransition() { sendTransition(); }

// {"transition": "random" | "none" | a name from the available list}
static void handlePutTransition() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["transition"].is<const char *>()) {
    sendError(400, "transition must be \"random\", \"none\" or a name from the available list");
    return;
  }
  String error;
  if (!transitionSetChoice(body["transition"].as<String>(), error)) {
    sendError(400, error.c_str());
    return;
  }
  sendTransition();
}

static void handleCard() {
  JsonDocument doc;
  cardReport(doc["card"].to<JsonObject>());
  artCacheReport(doc["cache"].to<JsonObject>());
  sendJson(200, doc);
}

// --- the gallery of artworks cached on the card --------------------------------

static const uint32_t GALLERY_PAGE_DEFAULT = 24;
static const uint32_t GALLERY_PAGE_MAX = 60;

// A whole-number query argument, or `fallback` when it is absent or not a number.
static uint32_t numberArg(const char *name, uint32_t fallback) {
  String text = server.arg(name);
  if (text.isEmpty()) {
    return fallback;
  }
  char *end;
  unsigned long value = strtoul(text.c_str(), &end, 10);
  return *end == '\0' ? value : fallback;
}

// The postId in the path, or 0 when it is not a plain number.
static uint32_t pathPostId() {
  String text = server.pathArg(0);
  if (text.isEmpty() || text.length() > 9) {
    return 0;
  }
  for (char c : text) {
    if (!isdigit(c)) {
      return 0;
    }
  }
  return text.toInt();
}

static void handleListArt() {
  JsonDocument doc;
  uint32_t limit = numberArg("limit", GALLERY_PAGE_DEFAULT);
  artCacheList(numberArg("offset", 0), limit < 1 ? 1 : min(limit, GALLERY_PAGE_MAX), doc.to<JsonObject>());
  sendJson(200, doc);
}

static void handleArtImage() {
  uint32_t postId = pathPostId();
  uint8_t *data = nullptr;
  size_t size = 0;
  uint32_t startedAt = millis();
  if (!postId || !artCacheReadFile(postId, data, size)) {
    sendError(404, "That artwork is not on the card");
    return;
  }
  uint32_t readAt = millis();
  // An artwork never changes once saved, so the browser may keep it.
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/gif", (const char *)data, size);
  free(data);
  slowestReadMs = max(slowestReadMs, readAt - startedAt);
  slowestSendMs = max(slowestSendMs, (uint32_t)millis() - readAt);
}

static void handleShowArt() {
  uint32_t postId = pathPostId();
  if (!postId || !artCacheHas(postId)) {
    sendError(404, "That artwork is not on the card");
    return;
  }
  sendQueued(commandSend(CommandType::ShowArtwork, postId), "show");
}

// Any Makapix post, by id: from the card when it is there, otherwise fetched.
// Queued like the rest; whether it could be shown appears in GET /status.
static void handleShowPost() {
  uint32_t postId = pathPostId();
  if (!postId) {
    sendError(404, "Not a postId");
    return;
  }
  sendQueued(commandSend(CommandType::ShowArtwork, postId), "show");
}

// --- the live connection to makapix.club ---------------------------------------------

static void sendLive() {
  JsonDocument doc;
  liveReport(doc.to<JsonObject>());
  sendJson(200, doc);
}

static void handleGetLive() { sendLive(); }

// {"enabled": false} says goodbye to the broker and stays off across restarts.
static void handlePutLive() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["enabled"].is<bool>()) {
    sendError(400, "enabled must be true or false");
    return;
  }
  liveSetEnabled(body["enabled"].as<bool>());
  sendLive();
}

static void handleBack() { sendQueued(commandSend(CommandType::Back), "back"); }

// --- test patterns -----------------------------------------------------------------

static void handleGetTest() {
  JsonDocument doc;
  doc["showing"] = statusGet().slot == "test";
  JsonArray patterns = doc["patterns"].to<JsonArray>();
  for (int i = 0; i < testpattern::COUNT; i++) {
    JsonObject item = patterns.add<JsonObject>();
    item["name"] = testpattern::name((testpattern::Pattern)i);
    item["label"] = testpattern::label((testpattern::Pattern)i);
  }
  sendJson(200, doc);
}

// {"pattern": "darks"}: shown until Next, DELETE /test or ten minutes.
static void handlePutTest() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  testpattern::Pattern pattern;
  if (!body["pattern"].is<const char *>() || !testpattern::fromName(body["pattern"], pattern)) {
    sendError(400, "pattern must be one of ramps, darks, bars, white");
    return;
  }
  sendQueued(commandSend(CommandType::ShowTestPattern, (uint32_t)pattern), "testPattern");
}

// Back to art: ends the pattern, as Next would, without skipping a slot.
static void handleDeleteTest() {
  if (statusGet().slot != "test") {
    JsonDocument doc;
    doc["showing"] = false;
    sendJson(200, doc);
    return;
  }
  sendQueued(commandSend(CommandType::Reload), "reload");
}

// --- one channel for every artwork slot, for now ------------------------------------

static void sendChannelOverride(int status) {
  JsonDocument doc;
  String from;
  String spec = rotationChannelOverride(&from);
  if (spec.isEmpty()) {
    doc["channel"] = nullptr;
    doc["label"] = nullptr;
    doc["from"] = nullptr;
  } else {
    channels::Channel channel;
    std::string error;
    channels::parse(spec.c_str(), channel, error);
    doc["channel"] = spec;
    doc["label"] = channels::label(channel).c_str();
    doc["from"] = from;
  }
  sendJson(status, doc);
}

static void handleGetChannel() { sendChannelOverride(200); }

// {"channel": "hashtag:cat"}. Starts with the next artwork turn.
static void handlePutChannel() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["channel"].is<const char *>() || String(body["channel"].as<const char *>()).isEmpty()) {
    sendError(400, "channel must be a channel string, such as \"promoted\" or \"hashtag:cat\"");
    return;
  }
  String error;
  if (!rotationSetChannelOverride(body["channel"].as<String>(), "api", error)) {
    sendError(400, error.c_str());
    return;
  }
  sendChannelOverride(200);
}

static void handleDeleteChannel() {
  String error;
  rotationSetChannelOverride("", "", error);
  sendChannelOverride(200);
}

static void handleRemoveArt() {
  uint32_t postId = pathPostId();
  if (!postId || !artCacheRemove(postId)) {
    sendError(404, "That artwork is not on the card");
    return;
  }
  JsonDocument doc;
  doc["removed"] = postId;
  sendJson(200, doc);
}

static void handleClearArt() {
  JsonDocument doc;
  doc["removed"] = artCacheClear();
  sendJson(200, doc);
}

// --- likes and view reports ------------------------------------------------------

static void handleGetLikes() {
  JsonDocument doc;
  reporterReportLikes(doc.to<JsonObject>());
  JsonArray liked = doc["liked"].to<JsonArray>();
  for (uint32_t postId : reporterLikes()) {
    liked.add(postId);
  }
  sendJson(200, doc);
}

static void setLike(bool liked) {
  uint32_t postId = pathPostId();
  if (!postId) {
    sendError(404, "Not a postId");
    return;
  }
  if (!makapix::isRegistered()) {
    sendError(409, "The panel is not linked to a Makapix account yet");
    return;
  }
  reporterSetLiked(postId, liked);
  JsonDocument doc;
  doc["postId"] = postId;
  doc["liked"] = liked;
  sendJson(200, doc);
}

static void handleLike() { setLike(true); }
static void handleUnlike() { setLike(false); }

static void sendViews() {
  JsonDocument doc;
  reporterReportViews(doc.to<JsonObject>());
  sendJson(200, doc);
}

static void handleGetViews() { sendViews(); }

// {"report": true|false}. Turning reports off also drops views still waiting.
static void handlePutViews() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["report"].is<bool>()) {
    sendError(400, "report must be true or false");
    return;
  }
  reporterSetViewsEnabled(body["report"].as<bool>());
  sendViews();
}

// --- crash records ----------------------------------------------------------------

static void handleGetCrash() {
  JsonDocument doc;
  crashReport(doc.to<JsonObject>());
  sendJson(200, doc);
}

// The raw record from flash, for espcoredump.py. Sent in pieces: it can be
// up to 64 KB and internal RAM is short.
static void handleCrashDump() {
  size_t size = crashDumpSize();
  if (!size) {
    sendError(404, "No crash record");
    return;
  }
  static const size_t PIECE = 1024;
  uint8_t *buffer = (uint8_t *)malloc(PIECE);
  if (!buffer) {
    sendError(503, "Out of memory, try again");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=\"pixel-jukebox-crash.bin\"");
  server.setContentLength(size);
  server.send(200, "application/octet-stream", "");
  for (size_t offset = 0; offset < size; offset += PIECE) {
    size_t length = min(PIECE, size - offset);
    if (!crashDumpRead(offset, buffer, length)) {
      break;
    }
    server.sendContent((const char *)buffer, length);
  }
  free(buffer);
}

static void handleDeleteCrash() {
  bool had = crashPresent();
  crashErase();
  JsonDocument doc;
  doc["erased"] = had;
  sendJson(200, doc);
}

#ifdef CRASH_TEST
// Only in a build made with -DCRASH_TEST, to check the crash record works.
static void handleCrashTest() {
  sendQueued(true, "crash");
  delay(200);
  volatile uint32_t *nowhere = nullptr;
  *nowhere = 1; // StoreProhibited
}
#endif

// --- photo mode -------------------------------------------------------------------

static void handleGetPanel() {
  JsonDocument doc;
  doc["photo"] = displayMode() == PanelMode::Photo;
  doc["refreshHz"] = display->refreshHz();
  orientation::Orientation turned = displayWantedOrientation();
  doc["rotation"] = orientation::toDegrees(turned);
  doc["mirror"] = turned.mirror;
  sendJson(200, doc);
}

// Any of {"photo": true, "rotation": 90, "mirror": false}. The orientation
// changes at once; photo mode is used from the next start-up, so changing it
// restarts the board.
static void handlePutPanel() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (body["photo"].isNull() && body["rotation"].isNull() && body["mirror"].isNull()) {
    sendError(400, "send photo, rotation or mirror");
    return;
  }
  if (!body["photo"].isNull() && !body["photo"].is<bool>()) {
    sendError(400, "photo must be true or false");
    return;
  }
  orientation::Orientation turned = displayWantedOrientation();
  if (!body["rotation"].isNull() &&
      (!body["rotation"].is<int>() || !orientation::fromDegrees(body["rotation"].as<int>(), turned.quarterTurns))) {
    sendError(400, "rotation must be 0, 90, 180 or 270");
    return;
  }
  if (!body["mirror"].isNull()) {
    if (!body["mirror"].is<bool>()) {
      sendError(400, "mirror must be true or false");
      return;
    }
    turned.mirror = body["mirror"];
  }
  if (!(turned == displayWantedOrientation())) {
    if (!commandSend(CommandType::SetOrientation, orientation::pack(turned))) {
      sendError(503, "Command queue is full, try again");
      return;
    }
    displayNoteWantedOrientation(turned);
  }

  PanelMode mode = body["photo"].isNull() ? displayMode()
                   : body["photo"].as<bool>() ? PanelMode::Photo
                                              : PanelMode::Normal;
  if (mode == displayMode()) {
    handleGetPanel();
    return;
  }
  displaySaveMode(mode);
  if (!commandSend(CommandType::Restart)) {
    displaySaveMode(displayMode());
    sendError(503, "Command queue is full, try again");
    return;
  }
  JsonDocument doc;
  doc["photo"] = mode == PanelMode::Photo;
  doc["rotation"] = orientation::toDegrees(turned);
  doc["mirror"] = turned.mirror;
  doc["restarting"] = true;
  sendJson(202, doc);
}

// --- the night schedule ------------------------------------------------------------

static void sendNight(int status) {
  JsonDocument doc;
  nightReport(doc.to<JsonObject>());
  sendJson(status, doc);
}

static void handleGetNight() { sendNight(200); }

// Any of {"enabled", "start", "end", "action", "brightness", "fadeMinutes"};
// the rest stay as they are.
static void handlePutNight() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  night::Settings settings = nightSettings();
  if (!body["enabled"].isNull()) {
    if (!body["enabled"].is<bool>()) {
      sendError(400, "enabled must be true or false");
      return;
    }
    settings.enabled = body["enabled"];
  }
  for (const char *field : {"start", "end"}) {
    if (body[field].isNull()) {
      continue;
    }
    uint16_t minutes;
    if (!body[field].is<const char *>() || !night::parseTime(body[field].as<const char *>(), minutes)) {
      sendError(400, "start and end are times like \"22:00\"");
      return;
    }
    (strcmp(field, "start") == 0 ? settings.start : settings.end) = minutes;
  }
  if (!body["action"].isNull()) {
    if (!body["action"].is<const char *>() || !night::actionFromName(body["action"].as<const char *>(), settings.action)) {
      sendError(400, "action must be \"dim\", \"off\" or \"clock\"");
      return;
    }
  }
  if (!body["brightness"].isNull()) {
    if (!body["brightness"].is<int>() || body["brightness"].as<int>() < 0 || body["brightness"].as<int>() > 255) {
      sendError(400, "brightness must be 0-255");
      return;
    }
    settings.brightness = body["brightness"].as<int>();
  }
  if (!body["fadeMinutes"].isNull()) {
    if (!body["fadeMinutes"].is<int>() || body["fadeMinutes"].as<int>() < 0 ||
        body["fadeMinutes"].as<int>() > night::MAX_FADE_MINUTES) {
      sendError(400, "fadeMinutes must be 0-60");
      return;
    }
    settings.fadeMinutes = body["fadeMinutes"].as<int>();
  }
  String error;
  if (!nightSetSettings(settings, error)) {
    sendError(400, error.c_str());
    return;
  }
  sendNight(200);
}

static void handleRestart() { sendQueued(commandSend(CommandType::Restart), "restart"); }

static void handleWifiSetup() { sendQueued(commandSend(CommandType::WifiSetup), "wifiSetup"); }

static void sendSlots() {
  JsonDocument doc;
  JsonArray available = doc["available"].to<JsonArray>();
  for (const SlotInfo &info : rotationAvailable()) {
    JsonObject item = available.add<JsonObject>();
    item["name"] = info.name;
    item["label"] = info.label;
    item["turn"] = info.turnMeaning;
    if (info.timed) {
      // A turn lasts the row's `seconds`.
      JsonObject seconds = item["seconds"].to<JsonObject>();
      seconds["min"] = ROTATION_SECONDS_MIN;
      seconds["max"] = ROTATION_SECONDS_MAX;
      seconds["default"] = ROTATION_SECONDS_DEFAULT;
    }
  }
  JsonArray rotation = doc["rotation"].to<JsonArray>();
  for (const RotationEntry &entry : rotationGet()) {
    JsonObject item = rotation.add<JsonObject>();
    item["slot"] = entry.slot;
    item["turns"] = entry.turns;
    item["enabled"] = entry.enabled;
    if (entry.seconds) {
      item["seconds"] = entry.seconds;
    }
    if (entry.slot == "makapix") {
      item["channel"] = entry.channel;
      channels::Channel channel;
      std::string error;
      channels::parse(entry.channel.c_str(), channel, error);
      item["channelLabel"] = channels::label(channel);
      String problem = makapixChannelProblem(entry.channel);
      if (!problem.isEmpty()) {
        item["problem"] = problem;
      }
    }
  }
  // What an artwork slot's channel can be; `needs` is what has to follow the colon.
  JsonArray channelTypes = doc["channels"].to<JsonArray>();
  const char *const TYPES[][3] = {{"all", "Everything", ""},          {"promoted", "Promoted", ""},
                                  {"mine", "My art", ""},             {"likedby", "Liked by", "name"},
                                  {"artist", "An artist", "name"},    {"hashtag", "A hashtag", "hashtag"},
                                  {"uploads", "My uploads", ""},      {"starter", "Starter art", ""}};
  for (const auto &type : TYPES) {
    JsonObject item = channelTypes.add<JsonObject>();
    item["type"] = type[0];
    item["label"] = type[1];
    if (type[2][0]) {
      item["needs"] = type[2];
    }
  }
  doc["maxEntries"] = ROTATION_MAX_ENTRIES;
  doc["maxTurns"] = ROTATION_MAX_TURNS;
  sendJson(200, doc);
}

static void handleGetSlots() { sendSlots(); }

// Replaces the whole rotation. Applied directly rather than queued: it takes
// effect when the slot on screen finishes.
static void handlePutSlots() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["rotation"].is<JsonArray>()) {
    sendError(400, "rotation must be an array");
    return;
  }

  std::vector<RotationEntry> entries;
  for (JsonVariant item : body["rotation"].as<JsonArray>()) {
    if (!item["slot"].is<const char *>() || !item["turns"].is<int>()) {
      sendError(400, "each entry needs a slot name and a whole number of turns");
      return;
    }
    if (!item["enabled"].isNull() && !item["enabled"].is<bool>()) {
      sendError(400, "enabled must be true or false");
      return;
    }
    if (!item["seconds"].isNull() && (!item["seconds"].is<int>() || item["seconds"].as<int>() < 1 ||
                                      item["seconds"].as<int>() > 255)) {
      sendError(400, "seconds must be a whole number of seconds, 5 to 60");
      return;
    }
    if (!item["channel"].isNull() && !item["channel"].is<const char *>()) {
      sendError(400, "channel must be a string, such as \"promoted\" or \"artist:Fab\"");
      return;
    }
    int turns = item["turns"];
    if (turns < 1 || turns > ROTATION_MAX_TURNS) {
      sendError(400, "turns is out of range");
      return;
    }
    String channel = item["channel"] | "";
    if (channel.isEmpty() && item["slot"].as<String>() == "makapix") {
      channel = "all";
    }
    entries.push_back(
        {item["slot"].as<String>(), (uint8_t)turns, channel, item["enabled"] | true, (uint8_t)(item["seconds"] | 0)});
  }

  String error;
  if (!rotationSet(entries, error)) {
    sendError(400, error.c_str());
    return;
  }
  sendSlots();
}

// --- raw request bodies: new firmware and uploads -------------------------------------

// Firmware and uploaded art arrive as the raw request body
// (application/octet-stream) and are written out as they come, so they never
// have to fit in memory. The PIN is checked once, before anything is written;
// the reply comes after the end. The server takes one request at a time, so
// the state below is shared.
static bool bodyReceived = false; // the body went through a raw handler
static AuthResult bodyAuth = AuthResult::Missing;
static int bodyStatus = 0; // an error's status, 0 when all is well
static String bodyError;

static void failBody(int status, const String &error) {
  if (bodyStatus == 0) {
    bodyStatus = status;
    bodyError = error;
  }
}

// Starts a raw body: notes the PIN check and the length. False when nothing
// should be kept (the PIN is missing, say); the body must still be read.
static bool beginBody(int &length) {
  bodyReceived = true;
  bodyStatus = 0;
  bodyError = "";
  bodyAuth = requestAuth();
  length = server.clientContentLength();
  return bodyAuth == AuthResult::Allowed;
}

// The web server's own raw-body loop asks for whole 1436-byte pieces, so on
// the last, shorter piece it spins for its 5 s timeout without yielding and the
// watchdog restarts the board. So the body is read here, at the start, taking
// whatever has arrived and waiting politely when nothing has; then the server
// is told the body is used up, which skips its loop. Each piece goes to
// `keep` until it reports a problem; after that the rest is read and dropped.
static const uint32_t BODY_STALL_MS = 10000;
static uint8_t bodyBuffer[4096];

static bool receiveBody(HTTPRaw &raw, int length, std::function<bool(uint8_t *, size_t, String &)> keep) {
  WiFiClient client = server.client();
  size_t remaining = length > 0 ? length : 0;
  uint32_t lastData = millis();
  bool complete = true;
  while (remaining > 0) {
    int ready = client.available();
    if (ready > 0) {
      size_t want = min(min((size_t)ready, sizeof(bodyBuffer)), remaining);
      int got = client.read(bodyBuffer, want);
      if (got <= 0) {
        continue;
      }
      remaining -= got;
      lastData = millis();
      String error;
      if (keep && bodyStatus == 0 && !keep(bodyBuffer, got, error)) {
        failBody(400, error); // almost always the wrong file
      }
    } else if (!client.connected() || millis() - lastData > BODY_STALL_MS) {
      complete = false;
      break;
    } else {
      delay(2);
    }
  }
  raw.totalSize = length > 0 ? length : 0; // the body is used up: the server skips its own loop
  if (!complete) {
    failBody(400, "The upload stopped part way; nothing was changed");
  }
  return complete;
}

// The reply's first part, shared: false (with the reply sent) when the body
// never came through a raw handler, the PIN was wrong, or something failed.
static bool bodyOk(const char *expected) {
  bool received = bodyReceived;
  bodyReceived = false;
  if (!received) {
    sendError(400, (String("Send ") + expected + " as the body, with Content-Type: application/octet-stream").c_str());
    return false;
  }
  if (bodyAuth != AuthResult::Allowed) {
    sendAuthRefusal(bodyAuth);
    return false;
  }
  if (bodyStatus != 0) {
    sendError(bodyStatus, bodyError.c_str());
    return false;
  }
  return true;
}

static void handleUpdateBody() {
  HTTPRaw &raw = server.raw();
  if (raw.status != RAW_START) {
    return; // the whole body was read at the start
  }
  int length;
  bool keep = false;
  if (beginBody(length)) {
    String error;
    if (length <= 0) {
      failBody(400, "Send firmware.bin as the request body");
    } else if (!firmwareUpdateStart(length, server.header("X-Firmware-MD5"), error)) {
      failBody(firmwareUpdateActive() || error.startsWith("The last update") ? 409 : 400, error);
    } else {
      keep = true;
    }
  }
  receiveBody(raw, length, keep ? std::function<bool(uint8_t *, size_t, String &)>(firmwareUpdateWrite) : nullptr);
}

static void handleUpdate() {
  if (!bodyOk("firmware.bin")) {
    firmwareUpdateAbort();
    return;
  }
  String error;
  if (!firmwareUpdateFinish(error)) {
    sendError(400, error.c_str());
    return;
  }
  JsonDocument doc;
  doc["updated"] = true;
  doc["restarting"] = true;
  sendJson(200, doc);
  commandSend(CommandType::Restart);
}

// --- uploads -------------------------------------------------------------------------

static void addUpload(JsonArray list, const UploadInfo &info) {
  JsonObject item = list.add<JsonObject>();
  item["id"] = info.id;
  item["name"] = info.name;
  item["width"] = info.width;
  item["height"] = info.height;
  item["bytes"] = info.bytes;
}

static void handleListUploads() {
  JsonDocument doc;
  uploadsReport(doc.to<JsonObject>());
  JsonArray list = doc["uploads"].to<JsonArray>();
  for (const UploadInfo &info : uploadsList()) {
    addUpload(list, info);
  }
  sendJson(200, doc);
}

// The file's name comes in X-File-Name, percent-encoded as browsers must for
// a header (encodeURIComponent).
static void handleUploadBody() {
  HTTPRaw &raw = server.raw();
  if (raw.status != RAW_START) {
    return;
  }
  int length;
  bool keep = false;
  if (beginBody(length)) {
    String error;
    String name = uploads::percentDecode(server.header("X-File-Name").c_str()).c_str();
    if (length <= 0) {
      failBody(400, "Send a GIF as the request body");
    } else if (!uploadsStart(length, name, error)) {
      failBody(error.startsWith("Another") ? 409 : error.startsWith("The card is full") ? 507 : 400, error);
    } else {
      keep = true;
    }
  }
  receiveBody(raw, length,
              keep ? std::function<bool(uint8_t *, size_t, String &)>(
                         [](uint8_t *data, size_t size, String &error) { return uploadsWrite(data, size, error); })
                   : nullptr);
}

static void handleUpload() {
  if (!bodyOk("a GIF")) {
    uploadsAbort();
    return;
  }
  UploadInfo info;
  String error;
  if (!uploadsFinish(info, error)) {
    sendError(400, error.c_str());
    return;
  }
  JsonDocument doc;
  JsonArray one = doc["uploads"].to<JsonArray>();
  addUpload(one, info);
  JsonDocument reply;
  reply.set(doc["uploads"][0]);
  sendJson(201, reply);
}

static uint32_t pathUploadId() {
  String text = server.pathArg(0);
  if (text.isEmpty() || text.length() > 9) {
    return 0;
  }
  for (char c : text) {
    if (!isdigit(c)) {
      return 0;
    }
  }
  return text.toInt();
}

static void handleUploadImage() {
  uint32_t id = pathUploadId();
  uint8_t *data = nullptr;
  size_t size = 0;
  if (!id || !uploadsRead(id, data, size)) {
    sendError(404, "No such upload");
    return;
  }
  server.sendHeader("Cache-Control", "max-age=86400"); // an upload never changes under its id
  server.send_P(200, "image/gif", (const char *)data, size);
  free(data);
}

// --- the clock -------------------------------------------------------------------------

static void sendClock() {
  clockface::Settings s = clockSettings();
  JsonDocument doc;
  doc["style"] = clockface::styleName(s.style);
  doc["hours"] = s.hours24 ? 24 : 12;
  doc["seconds"] = s.seconds;
  doc["date"] = clockface::dateName(s.date);
  doc["color"] = clockface::colorName(s.color);
  doc["timezone"] = timezones::at(timezoneIndex()).name;
  struct tm now;
  if (getLocalTime(&now, 0)) {
    char local[20];
    strftime(local, sizeof(local), "%Y-%m-%d %H:%M", &now);
    doc["localTime"] = local; // as the clock shows it, to check the zone
  } else {
    doc["localTime"] = nullptr;
  }
  char hex[8];
  clockface::formatHex(s.custom, hex);
  doc["customColor"] = hex;
  JsonObject choices = doc["choices"].to<JsonObject>();
  JsonArray styles = choices["style"].to<JsonArray>();
  for (int i = 0; i < (int)clockface::Style::COUNT; i++) {
    styles.add(clockface::styleName((clockface::Style)i));
  }
  JsonArray dates = choices["date"].to<JsonArray>();
  for (int i = 0; i < (int)clockface::DateFormat::COUNT; i++) {
    dates.add(clockface::dateName((clockface::DateFormat)i));
  }
  JsonArray colors = choices["color"].to<JsonArray>();
  for (int i = 0; i < (int)clockface::Color::COUNT; i++) {
    colors.add(clockface::colorName((clockface::Color)i));
  }
  JsonArray zones = choices["timezone"].to<JsonArray>();
  for (size_t i = 0; i < timezones::count(); i++) {
    JsonObject zone = zones.add<JsonObject>();
    zone["name"] = timezones::at(i).name;
    zone["label"] = timezones::at(i).label;
  }
  sendJson(200, doc);
}

static void handleGetClock() { sendClock(); }

// Any of {"style", "hours": 12 or 24, "seconds", "date", "color", "customColor",
// "timezone"}; the rest
// stay as they are. A clock on screen shows the change at once.
static void handlePutClock() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  clockface::Settings s = clockSettings();
  if (!body["style"].isNull() && !(body["style"].is<const char *>() && clockface::styleFrom(body["style"].as<const char *>(), s.style))) {
    sendError(400, "style must be digital, big or analog");
    return;
  }
  if (!body["hours"].isNull()) {
    int hours = body["hours"].is<int>() ? body["hours"].as<int>() : 0;
    if (hours != 12 && hours != 24) {
      sendError(400, "hours must be 12 or 24");
      return;
    }
    s.hours24 = hours == 24;
  }
  if (!body["seconds"].isNull()) {
    if (!body["seconds"].is<bool>()) {
      sendError(400, "seconds must be true or false");
      return;
    }
    s.seconds = body["seconds"];
  }
  if (!body["date"].isNull() && !(body["date"].is<const char *>() && clockface::dateFrom(body["date"].as<const char *>(), s.date))) {
    sendError(400, "date must be off, weekday, daymonth or numeric");
    return;
  }
  if (!body["color"].isNull() &&
      !(body["color"].is<const char *>() && clockface::colorFrom(body["color"].as<const char *>(), s.color))) {
    sendError(400, "color must be white, amber, mint, timeofday or custom");
    return;
  }
  if (!body["customColor"].isNull() &&
      !(body["customColor"].is<const char *>() &&
        clockface::parseHex(body["customColor"].as<const char *>(), s.custom))) {
    sendError(400, "customColor must be like \"#ff8800\"");
    return;
  }
  int zone = -1;
  if (!body["timezone"].isNull()) {
    zone = body["timezone"].is<const char *>() ? timezones::find(body["timezone"].as<const char *>()) : -1;
    if (zone < 0) {
      sendError(400, "timezone must be one of choices.timezone, such as \"America/Chicago\"");
      return;
    }
  }
  if (zone >= 0 && zone != timezoneIndex()) {
    if (!commandSend(CommandType::ApplyTimezone)) {
      sendError(503, "Command queue is full, try again");
      return;
    }
    timezoneSave(zone); // saved now; the display loop puts it in force
  }
  clockSetSettings(s);
  sendClock();
}

// Shows the clock for one turn, to see how it looks.
static void handleShowClock() { sendQueued(commandSend(CommandType::ShowClock), "showClock"); }

// --- the weather -------------------------------------------------------------------

static void sendWeather() {
  JsonDocument doc;
  weatherReport(doc.to<JsonObject>());
  sendJson(200, doc);
}

static void handleGetWeather() { sendWeather(); }

// Any of {"key", "place": {"name", "region", "country", "lat", "lon"}, "units":
// "f" or "c", "refreshMinutes"}; the rest stay as they are. The key is never
// sent back, only whether one is set.
static void handlePutWeather() {
  JsonDocument body;
  if (!readBody(body)) {
    return;
  }
  if (!body["key"].isNull()) {
    String key = body["key"] | "";
    key.trim();
    bool plain = body["key"].is<const char *>() && key.length() <= 64;
    for (char c : key) {
      plain = plain && isalnum((unsigned char)c);
    }
    if (!plain) {
      sendError(400, "key must be letters and digits, as WeatherAPI.com gives it");
      return;
    }
  }
  WeatherPlace where;
  bool setPlace = !body["place"].isNull();
  if (setPlace) {
    JsonObject p = body["place"];
    if (!p["name"].is<const char *>() || !p["lat"].is<float>() || !p["lon"].is<float>()) {
      sendError(400, "place needs a name, lat and lon, as GET /weather/search gives them");
      return;
    }
    float lat = p["lat"], lon = p["lon"];
    where.name = p["name"].as<String>().substring(0, 48);
    where.region = String(p["region"] | "").substring(0, 48);
    where.country = String(p["country"] | "").substring(0, 48);
    where.lat = lat;
    where.lon = lon;
    if (where.name.isEmpty() || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
      sendError(400, "place is not a place on Earth");
      return;
    }
  }
  if (!body["units"].isNull() && body["units"] != "f" && body["units"] != "c") {
    sendError(400, "units must be \"f\" or \"c\"");
    return;
  }
  if (!body["refreshMinutes"].isNull()) {
    String error;
    if (!body["refreshMinutes"].is<int>() || !weatherSetRefresh(body["refreshMinutes"].as<int>(), error)) {
      sendError(400, error.isEmpty() ? String("refreshMinutes must be a whole number").c_str() : error.c_str());
      return;
    }
  }
  if (!body["key"].isNull()) {
    String key = body["key"] | "";
    key.trim();
    weatherSetKey(key);
  }
  if (setPlace) {
    weatherSetPlace(where);
  }
  if (!body["units"].isNull()) {
    weatherSetFahrenheit(body["units"] == "f");
  }
  sendWeather();
}

// ?q=George: towns for Settings to choose from. Asks WeatherAPI.com, so it
// takes a second or two.
static void handleWeatherSearch() {
  std::vector<WeatherPlace> places;
  String error;
  if (!weatherSearch(server.arg("q"), places, error)) {
    sendError(error.startsWith("type ") || error.startsWith("enter ") ? 400 : 502, error.c_str());
    return;
  }
  JsonDocument doc;
  JsonArray list = doc["places"].to<JsonArray>();
  for (const WeatherPlace &where : places) {
    JsonObject item = list.add<JsonObject>();
    item["name"] = where.name;
    item["region"] = where.region;
    item["country"] = where.country;
    item["lat"] = where.lat;
    item["lon"] = where.lon;
  }
  sendJson(200, doc);
}

// --- starter art ---------------------------------------------------------------------

static void handleListStarter() {
  JsonDocument doc;
  starterArtReport(doc.to<JsonObject>());
  JsonArray list = doc["pieces"].to<JsonArray>();
  for (uint16_t id = 1; id <= starterArtCount(); id++) {
    StarterPiece piece;
    starterArtFind(id, piece);
    JsonObject item = list.add<JsonObject>();
    item["id"] = piece.id;
    item["title"] = piece.title;
    item["credit"] = piece.credit;
  }
  sendJson(200, doc);
}

// The id in /starter/{id}..., or 0.
static uint16_t pathStarterId() {
  String text = server.pathArg(0);
  if (text.isEmpty() || text.length() > 3) {
    return 0;
  }
  for (char c : text) {
    if (!isdigit(c)) {
      return 0;
    }
  }
  return text.toInt();
}

static void sendStarterImage(bool still) {
  const uint8_t *data;
  size_t size;
  uint16_t id = pathStarterId();
  if (!id || !starterArtBytes(id, data, size, still)) {
    sendError(404, "No such piece");
    return;
  }
  server.sendHeader("Cache-Control", "max-age=3600"); // the same id may hold other art after an update
  server.send_P(200, "image/gif", (const char *)data, size);
}

static void handleStarterImage() { sendStarterImage(false); }
static void handleStarterStill() { sendStarterImage(true); }

static void handleShowStarter() {
  uint16_t id = pathStarterId();
  StarterPiece piece;
  if (!id || !starterArtFind(id, piece)) {
    sendError(404, "No such piece");
    return;
  }
  sendQueued(commandSend(CommandType::ShowStarter, id), "showStarter");
}

static void handleShowUpload() {
  uint32_t id = pathUploadId();
  UploadInfo info;
  if (!id || !uploadsFind(id, info)) {
    sendError(404, "No such upload");
    return;
  }
  sendQueued(commandSend(CommandType::ShowUpload, id), "showUpload");
}

static void handleRemoveUpload() {
  uint32_t id = pathUploadId();
  if (!id || !uploadsRemove(id)) {
    sendError(404, "No such upload");
    return;
  }
  JsonDocument doc;
  doc["removed"] = id;
  sendJson(200, doc);
}

static void handleNotFound() { sendError(404, "No such endpoint"); }

static void serverTask(void *) {
  for (;;) {
    uint32_t startedAt = millis();
    server.handleClient();
    uint32_t took = millis() - startedAt;
    if (took >= 1000) {
      slowCount++;
    }
    if (took > slowestMs) {
      slowestMs = took;
      slowestUri = server.uri();
    }
    MEMORY_CHECK("api");
    vTaskDelay(pdMS_TO_TICKS(SERVER_POLL_MS));
  }
}

bool apiRunning() { return started && WiFi.status() == WL_CONNECTED; }

void apiBegin() {
  if (started) {
    return;
  }
  started = true;

  server.on("/", HTTP_GET, handlePage);
  // Open, like the page itself. Browsers also ask for /favicon.ico on their own.
  server.on("/favicon.png", HTTP_GET, []() { sendFavicon(server); });
  server.on("/favicon.ico", HTTP_GET, []() { sendFavicon(server); });
  server.on("/gallery", HTTP_GET, handleGalleryPage);
  server.on(API_BASE, HTTP_GET, handleApiRoot);
  server.on(API_BASE "/status", HTTP_GET, guarded(handleStatus));
  server.on(API_BASE "/diagnostics", HTTP_GET, guarded(handleDiagnostics));
  server.on(API_BASE "/next", HTTP_POST, guarded(handleNext));
  server.on(API_BASE "/back", HTTP_POST, guarded(handleBack));
  server.on(API_BASE "/test", HTTP_GET, guarded(handleGetTest));
  server.on(API_BASE "/test", HTTP_PUT, guarded(handlePutTest));
  server.on(API_BASE "/test", HTTP_DELETE, guarded(handleDeleteTest));
  server.on(API_BASE "/live", HTTP_GET, guarded(handleGetLive));
  server.on(API_BASE "/live", HTTP_PUT, guarded(handlePutLive));
  server.on(API_BASE "/channel", HTTP_GET, guarded(handleGetChannel));
  server.on(API_BASE "/channel", HTTP_PUT, guarded(handlePutChannel));
  server.on(API_BASE "/channel", HTTP_DELETE, guarded(handleDeleteChannel));
  server.on(UriBraces(API_BASE "/show/{}"), HTTP_POST, guarded(handleShowPost));
  server.on(API_BASE "/hold", HTTP_POST, guarded(handleHold));
  server.on(API_BASE "/brightness", HTTP_GET, guarded(handleGetBrightness));
  server.on(API_BASE "/brightness", HTTP_PUT, guarded(handlePutBrightness));
  server.on(API_BASE "/night", HTTP_GET, guarded(handleGetNight));
  server.on(API_BASE "/night", HTTP_PUT, guarded(handlePutNight));
  server.on(API_BASE "/panel", HTTP_GET, guarded(handleGetPanel));
  server.on(API_BASE "/panel", HTTP_PUT, guarded(handlePutPanel));
  server.on(API_BASE "/mode", HTTP_GET, guarded(handleGetMode));
  server.on(API_BASE "/mode", HTTP_PUT, guarded(handlePutMode));
  server.on(API_BASE "/transition", HTTP_GET, guarded(handleGetTransition));
  server.on(API_BASE "/transition", HTTP_PUT, guarded(handlePutTransition));
  server.on(API_BASE "/card", HTTP_GET, guarded(handleCard));
  server.on(API_BASE "/card/art", HTTP_GET, guarded(handleListArt));
  server.on(API_BASE "/card/art", HTTP_DELETE, guarded(handleClearArt));
  server.on(UriBraces(API_BASE "/card/art/{}.gif"), HTTP_GET, guarded(handleArtImage));
  server.on(UriBraces(API_BASE "/card/art/{}/show"), HTTP_POST, guarded(handleShowArt));
  server.on(UriBraces(API_BASE "/card/art/{}"), HTTP_DELETE, guarded(handleRemoveArt));
  server.on(API_BASE "/restart", HTTP_POST, guarded(handleRestart));
  server.on(API_BASE "/crash", HTTP_GET, guarded(handleGetCrash));
  server.on(API_BASE "/crash", HTTP_DELETE, guarded(handleDeleteCrash));
  server.on(API_BASE "/crash/dump", HTTP_GET, guarded(handleCrashDump));
#ifdef CRASH_TEST
  server.on("/crash-test", HTTP_POST, guarded(handleCrashTest));
#endif
  server.on(API_BASE "/likes", HTTP_GET, guarded(handleGetLikes));
  server.on(UriBraces(API_BASE "/likes/{}"), HTTP_PUT, guarded(handleLike));
  server.on(UriBraces(API_BASE "/likes/{}"), HTTP_DELETE, guarded(handleUnlike));
  server.on(API_BASE "/views", HTTP_GET, guarded(handleGetViews));
  server.on(API_BASE "/views", HTTP_PUT, guarded(handlePutViews));
  server.on(API_BASE "/wifi/setup", HTTP_POST, guarded(handleWifiSetup));
  server.on(API_BASE "/slots", HTTP_GET, guarded(handleGetSlots));
  server.on(API_BASE "/slots", HTTP_PUT, guarded(handlePutSlots));
  server.on(API_BASE "/auth", HTTP_GET, handleGetAuth);
  server.on(API_BASE "/auth/login", HTTP_POST, handleLogin);
  server.on(API_BASE "/auth/logout", HTTP_POST, handleLogout);
  server.on(API_BASE "/auth/pin", HTTP_PUT, guarded(handlePutPin));
  server.on(API_BASE "/update", HTTP_POST, handleUpdate, handleUpdateBody); // checks the PIN itself
  server.on(API_BASE "/uploads", HTTP_GET, guarded(handleListUploads));
  server.on(API_BASE "/clock", HTTP_GET, guarded(handleGetClock));
  server.on(API_BASE "/clock", HTTP_PUT, guarded(handlePutClock));
  server.on(API_BASE "/clock/show", HTTP_POST, guarded(handleShowClock));
  server.on(API_BASE "/weather", HTTP_GET, guarded(handleGetWeather));
  server.on(API_BASE "/weather", HTTP_PUT, guarded(handlePutWeather));
  server.on(API_BASE "/weather/search", HTTP_GET, guarded(handleWeatherSearch));
  server.on(API_BASE "/starter", HTTP_GET, guarded(handleListStarter));
  // Before {}.gif, which would match "7/still" as the id.
  server.on(UriBraces(API_BASE "/starter/{}/still.gif"), HTTP_GET, guarded(handleStarterStill));
  server.on(UriBraces(API_BASE "/starter/{}.gif"), HTTP_GET, guarded(handleStarterImage));
  server.on(UriBraces(API_BASE "/starter/{}/show"), HTTP_POST, guarded(handleShowStarter));
  server.on(API_BASE "/uploads", HTTP_POST, handleUpload, handleUploadBody); // checks the PIN itself
  server.on(UriBraces(API_BASE "/uploads/{}.gif"), HTTP_GET, guarded(handleUploadImage));
  server.on(UriBraces(API_BASE "/uploads/{}/show"), HTTP_POST, guarded(handleShowUpload));
  server.on(UriBraces(API_BASE "/uploads/{}"), HTTP_DELETE, guarded(handleRemoveUpload));
  server.onNotFound(handleNotFound);
  // The server keeps only the headers it is told to.
  static const char *headers[] = {"Cookie", "Authorization", AUTH_PIN_HEADER, "X-Firmware-MD5", "X-File-Name"};
  server.collectHeaders(headers, sizeof(headers) / sizeof(headers[0]));
  server.begin();

  // The TXT record lets a client tell a jukebox from any other web server.
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "jukebox", API_BASE);
  } else {
    Serial.println("mDNS failed to start");
  }

  TaskHandle_t task = nullptr;
  xTaskCreatePinnedToCore(serverTask, "api", SERVER_STACK_BYTES, nullptr, 1, &task, SERVER_CORE);
  telemetryRegisterTask("api", task, SERVER_STACK_BYTES);
  Serial.printf("API at http://%s.local%s\n", MDNS_HOSTNAME, API_BASE);
}
