#include "network/api.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <uri/UriBraces.h>

#include "app/clock_chip.h"
#include "app/commands.h"
#include "config.h"
#include "display/display.h"
#include "app/mode.h"
#include "app/rotation.h"
#include "app/status.h"
#include "app/telemetry.h"
#include "app/transition.h"
#include "network/makapix.h"
#include "network/reporter.h"
#include "network/web_assets.h"
#include "storage/art_cache.h"
#include "storage/card.h"
#include "transitions.h"

#define API_BASE "/api/v1"

static const uint32_t SERVER_STACK_BYTES = 8192;
static const BaseType_t SERVER_CORE = 0; // away from the display loop on core 1
static const uint32_t SERVER_POLL_MS = 2;

static WebServer server(80);
static bool started = false;

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

static void handlePage() { server.send_P(200, "text/html", WEB_INDEX_HTML); }

static void handleGalleryPage() { server.send_P(200, "text/html", WEB_GALLERY_HTML); }

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

  if (status.hasArtwork) {
    JsonObject artwork = doc["artwork"].to<JsonObject>();
    artwork["postId"] = status.postId;
    artwork["owner"] = status.owner;
    artwork["gifUrl"] = status.gifUrl;
    artwork["source"] = status.fromCache ? "card" : "makapix";
    artwork["liked"] = reporterLiked(status.postId);
  } else {
    doc["artwork"] = nullptr;
  }

  doc["mode"] = modeIsSound() ? "sound" : "rotation";
  doc["visualizer"] = modeVisualizer();
  doc["brightness"] = displayBrightness();
  sendJson(200, doc);
}

static void handleDiagnostics() {
  JsonDocument doc;
  telemetryReport(doc.to<JsonObject>());
  clockReport(doc["clock"].to<JsonObject>());
  cardReport(doc["card"].to<JsonObject>());
  artCacheReport(doc["cache"].to<JsonObject>());
  reporterReportViews(doc["views"].to<JsonObject>());
  reporterReportLikes(doc["likes"].to<JsonObject>());
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
  doc["value"] = displayBrightness();
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
  if (!postId || !artCacheReadFile(postId, data, size)) {
    sendError(404, "That artwork is not on the card");
    return;
  }
  // An artwork never changes once saved, so the browser may keep it.
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send_P(200, "image/gif", (const char *)data, size);
  free(data);
}

static void handleShowArt() {
  uint32_t postId = pathPostId();
  if (!postId || !artCacheHas(postId)) {
    sendError(404, "That artwork is not on the card");
    return;
  }
  if (modeIsSound()) {
    sendError(409, "The panel is in sound mode; switch to rotation to show artwork");
    return;
  }
  sendQueued(commandSend(CommandType::ShowArtwork, postId), "show");
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
  }
  JsonArray rotation = doc["rotation"].to<JsonArray>();
  for (const RotationEntry &entry : rotationGet()) {
    JsonObject item = rotation.add<JsonObject>();
    item["slot"] = entry.slot;
    item["turns"] = entry.turns;
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
    int turns = item["turns"];
    if (turns < 1 || turns > ROTATION_MAX_TURNS) {
      sendError(400, "turns is out of range");
      return;
    }
    entries.push_back({item["slot"].as<String>(), (uint8_t)turns});
  }

  String error;
  if (!rotationSet(entries, error)) {
    sendError(400, error.c_str());
    return;
  }
  sendSlots();
}

static void handleNotFound() { sendError(404, "No such endpoint"); }

static void serverTask(void *) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(SERVER_POLL_MS));
  }
}

void apiBegin() {
  if (started) {
    return;
  }
  started = true;

  server.on("/", HTTP_GET, handlePage);
  server.on("/gallery", HTTP_GET, handleGalleryPage);
  server.on(API_BASE, HTTP_GET, handleApiRoot);
  server.on(API_BASE "/status", HTTP_GET, handleStatus);
  server.on(API_BASE "/diagnostics", HTTP_GET, handleDiagnostics);
  server.on(API_BASE "/next", HTTP_POST, handleNext);
  server.on(API_BASE "/hold", HTTP_POST, handleHold);
  server.on(API_BASE "/brightness", HTTP_GET, handleGetBrightness);
  server.on(API_BASE "/brightness", HTTP_PUT, handlePutBrightness);
  server.on(API_BASE "/mode", HTTP_GET, handleGetMode);
  server.on(API_BASE "/mode", HTTP_PUT, handlePutMode);
  server.on(API_BASE "/transition", HTTP_GET, handleGetTransition);
  server.on(API_BASE "/transition", HTTP_PUT, handlePutTransition);
  server.on(API_BASE "/card", HTTP_GET, handleCard);
  server.on(API_BASE "/card/art", HTTP_GET, handleListArt);
  server.on(API_BASE "/card/art", HTTP_DELETE, handleClearArt);
  server.on(UriBraces(API_BASE "/card/art/{}.gif"), HTTP_GET, handleArtImage);
  server.on(UriBraces(API_BASE "/card/art/{}/show"), HTTP_POST, handleShowArt);
  server.on(UriBraces(API_BASE "/card/art/{}"), HTTP_DELETE, handleRemoveArt);
  server.on(API_BASE "/restart", HTTP_POST, handleRestart);
  server.on(API_BASE "/likes", HTTP_GET, handleGetLikes);
  server.on(UriBraces(API_BASE "/likes/{}"), HTTP_PUT, handleLike);
  server.on(UriBraces(API_BASE "/likes/{}"), HTTP_DELETE, handleUnlike);
  server.on(API_BASE "/views", HTTP_GET, handleGetViews);
  server.on(API_BASE "/views", HTTP_PUT, handlePutViews);
  server.on(API_BASE "/wifi/setup", HTTP_POST, handleWifiSetup);
  server.on(API_BASE "/slots", HTTP_GET, handleGetSlots);
  server.on(API_BASE "/slots", HTTP_PUT, handlePutSlots);
  server.onNotFound(handleNotFound);
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
