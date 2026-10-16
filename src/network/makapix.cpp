#include "network/makapix.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

#include "ca_certs.h"
#include "config.h"
#include "pins.h"

namespace makapix {

static const int PAGE_SIZE = 25;
static const uint16_t HTTP_TIMEOUT_MS = 15000;

static Preferences prefs;
static SemaphoreHandle_t lock; // one TLS session at a time: two at once can run internal RAM out
static String playerKey;
static String apiToken;

// Paging state for the current pass through the feed.
static String cursor;
static uint32_t shuffleSeed = 0;

static int requestLocked(const char *method, const String &path, const String &body, String &response) {
  WiFiClientSecure client;
  client.setCACert(MAKAPIX_ROOT_CAS);

  HTTPClient http;
  http.setUserAgent(USER_AGENT);
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(client, String(MAKAPIX_API_URL) + path)) {
    return -1;
  }
  if (apiToken.length()) {
    http.addHeader("Authorization", "Bearer " + apiToken);
  }

  int status;
  if (body.length()) {
    http.addHeader("Content-Type", "application/json");
    status = http.sendRequest(method, body);
  } else {
    status = http.sendRequest(method);
  }
  if (status > 0) {
    response = http.getString();
  }
  http.end();
  return status;
}

// Every call to Makapix goes through here, from the loader, the reporter and
// the display loop (registration), so they take turns.
static int request(const char *method, const String &path, const String &body, String &response) {
  xSemaphoreTake(lock, portMAX_DELAY);
  int status = requestLocked(method, path, body, response);
  xSemaphoreGive(lock);
  return status;
}

static void storeToken(const String &token) {
  apiToken = token;
  prefs.putString("api_token", apiToken);
}

void begin() {
  lock = xSemaphoreCreateMutex();
  prefs.begin("makapix", false);
  playerKey = prefs.getString("player_key", "");
  apiToken = prefs.getString("api_token", "");
}

bool isRegistered() { return apiToken.length() > 0; }

bool provision(String &registrationCode) {
  JsonDocument body;
  body["device_model"] = DEVICE_MODEL;
  body["firmware_version"] = FIRMWARE_VERSION;
  String payload;
  serializeJson(body, payload);

  String response;
  int status = request("POST", "/player/provision", payload, response);
  if (status != 201) {
    Serial.printf("Provision failed: HTTP %d\n", status);
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    return false;
  }
  playerKey = doc["player_key"].as<String>();
  registrationCode = doc["registration_code"].as<String>();
  prefs.putString("player_key", playerKey);
  return playerKey.length() > 0 && registrationCode.length() > 0;
}

// The token is only returned on the first credentials fetch; if it is missing
// (already fetched earlier), mint a new one.
static bool rotateToken() {
  String response;
  int status = request("POST", "/player/" + playerKey + "/token/rotate", "", response);
  JsonDocument doc;
  if (status != 200 || deserializeJson(doc, response) || doc["api_token"].isNull()) {
    Serial.printf("Token rotation failed: HTTP %d\n", status);
    return false;
  }
  storeToken(doc["api_token"].as<String>());
  return true;
}

LinkStatus pollCredentials() {
  String response;
  int status = request("GET", "/player/" + playerKey + "/credentials", "", response);
  if (status == 404) {
    return LinkStatus::Pending;
  }
  if (status == 429) {
    return LinkStatus::RateLimited;
  }
  if (status != 200) {
    return LinkStatus::Failed;
  }

  // The response also carries the MQTT certificates; only the token is needed.
  JsonDocument filter;
  filter["api_token"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) {
    return LinkStatus::Failed;
  }
  if (doc["api_token"].isNull()) {
    return rotateToken() ? LinkStatus::Linked : LinkStatus::Failed;
  }
  storeToken(doc["api_token"].as<String>());
  return LinkStatus::Linked;
}

// Almost every post has a server-converted GIF next to the native file, at the
// same vault path. Players may fetch from the vault over plain HTTP.
static String gifUrlFor(const String &artUrl) {
  String url = artUrl;
  int dot = url.lastIndexOf('.');
  if (dot > url.lastIndexOf('/')) {
    url = url.substring(0, dot);
  }
  url += ".gif";
  if (url.startsWith("https://")) {
    url = "http://" + url.substring(8);
  }
  return url;
}

QueryStatus nextPage(std::vector<Artwork> &artworks) {
  artworks.clear();
  if (cursor.isEmpty()) {
    shuffleSeed = esp_random() & 0x7fffffff; // new shuffle for each pass
  }

  JsonDocument body;
  body["request_type"] = "query_posts";
  body["channel"] = "all";
  body["sort"] = "random";
  body["random_seed"] = shuffleSeed;
  body["limit"] = PAGE_SIZE;
  if (!cursor.isEmpty()) {
    body["cursor"] = cursor;
  }
  JsonArray criteria = body["criteria"].to<JsonArray>();
  JsonObject fits = criteria.add<JsonObject>();
  fits["field"] = "width";
  fits["op"] = "lte";
  fits["value"] = PANEL_RES_X;
  JsonObject hasGif = criteria.add<JsonObject>();
  hasGif["field"] = "file_format";
  hasGif["op"] = "eq";
  hasGif["value"] = "gif";
  JsonArray fields = body["include_fields"].to<JsonArray>();
  fields.add("dwell_time_ms");
  fields.add("owner_handle");

  String payload;
  serializeJson(body, payload);
  String response;
  int status = request("POST", "/player/rpc", payload, response);
  if (status == 401) {
    return QueryStatus::Unauthorized;
  }

  JsonDocument doc;
  if (status != 200 || deserializeJson(doc, response) || !doc["success"].as<bool>()) {
    Serial.printf("Query failed: HTTP %d\n", status);
    return QueryStatus::Failed;
  }

  for (JsonObject post : doc["posts"].as<JsonArray>()) {
    Artwork artwork;
    artwork.postId = post["post_id"] | 0;
    artwork.ownerHandle = post["owner_handle"] | "";
    artwork.gifUrl = gifUrlFor(post["art_url"].as<String>());
    artwork.dwellMs = post["dwell_time_ms"] | 30000;
    artworks.push_back(artwork);
  }

  // An empty cursor makes the next call start a fresh pass.
  cursor = doc["has_more"].as<bool>() ? doc["next_cursor"].as<String>() : String();
  return QueryStatus::Ok;
}

// The emoji Makapix's own like button uses.
static const char *HEART = "\xE2\x9D\xA4\xEF\xB8\x8F"; // U+2764 U+FE0F

static SendStatus sendStatusFor(int status, const String &response) {
  if (status == 200 || status == 201 || status == 202) {
    return SendStatus::Ok;
  }
  if (status == 401) {
    return SendStatus::Unauthorized;
  }
  if (status == 429) {
    return SendStatus::RateLimited;
  }
  // A missing or deleted post, or a request Makapix will never accept as it
  // stands (too many reactions on one post, say): retrying cannot help.
  if (status == 400 || status == 404 || status == 410) {
    Serial.printf("Makapix refused: HTTP %d %s\n", status, response.c_str());
    return SendStatus::Gone;
  }
  return SendStatus::Failed;
}

SendStatus reportView(uint32_t postId, const String &isoTime, bool requested) {
  JsonDocument body;
  body["post_id"] = postId;
  body["timestamp"] = isoTime;
  body["timezone"] = "";
  body["intent"] = requested ? "artwork" : "channel";
  body["play_order"] = 2; // random
  body["channel"] = "all";
  String payload;
  serializeJson(body, payload);
  String response;
  return sendStatusFor(request("POST", "/player/events/view", payload, response), response);
}

SendStatus setLiked(uint32_t postId, bool liked) {
  JsonDocument body;
  body["request_type"] = liked ? "submit_reaction" : "revoke_reaction";
  body["post_id"] = postId;
  body["emoji"] = HEART;
  String payload;
  serializeJson(body, payload);
  String response;
  int status = request("POST", "/player/rpc", payload, response);
  SendStatus result = sendStatusFor(status, response);
  if (result == SendStatus::Ok) {
    JsonDocument doc;
    if (deserializeJson(doc, response) || !doc["success"].as<bool>()) {
      Serial.printf("Reaction refused: %s\n", response.c_str());
      return SendStatus::Gone;
    }
  }
  return result;
}

void forget() {
  prefs.remove("player_key");
  prefs.remove("api_token");
  playerKey = "";
  apiToken = "";
  cursor = "";
}

} // namespace makapix
