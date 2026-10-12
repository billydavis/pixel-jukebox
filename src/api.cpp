#include "api.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "commands.h"
#include "config.h"
#include "display.h"
#include "status.h"
#include "web_page.h"

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

static void handlePage() { server.send_P(200, "text/html", WEB_PAGE); }

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
  } else {
    doc["artwork"] = nullptr;
  }

  doc["brightness"] = displayBrightness();
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
  server.on(API_BASE, HTTP_GET, handleApiRoot);
  server.on(API_BASE "/status", HTTP_GET, handleStatus);
  server.on(API_BASE "/next", HTTP_POST, handleNext);
  server.on(API_BASE "/hold", HTTP_POST, handleHold);
  server.on(API_BASE "/brightness", HTTP_GET, handleGetBrightness);
  server.on(API_BASE "/brightness", HTTP_PUT, handlePutBrightness);
  server.onNotFound(handleNotFound);
  server.begin();

  // The TXT record lets a client tell a jukebox from any other web server.
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "jukebox", API_BASE);
  } else {
    Serial.println("mDNS failed to start");
  }

  xTaskCreatePinnedToCore(serverTask, "api", SERVER_STACK_BYTES, nullptr, 1, nullptr, SERVER_CORE);
  Serial.printf("API at http://%s.local%s\n", MDNS_HOSTNAME, API_BASE);
}
