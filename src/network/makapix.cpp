#include "network/makapix.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>

#include "ca_certs.h"
#include "config.h"
#include "app/memory_trace.h"
#include "pins.h"
#include "storage/card.h"

namespace makapix {

static const int PAGE_SIZE = 25;
static const uint16_t HTTP_TIMEOUT_MS = 15000;

static Preferences prefs;
static SemaphoreHandle_t lock; // one TLS session at a time: two at once can run internal RAM out
static String playerKey;
static String apiToken;


// Retry-After in whole seconds (Makapix sends it with 429), clamped to a sane
// range; 0 when absent or in a form we do not read (an HTTP date).
static uint32_t retryAfterSeconds(HTTPClient &http) {
  String value = http.header("Retry-After");
  value.trim();
  if (value.isEmpty() || value.length() > 6) {
    return 0;
  }
  for (char c : value) {
    if (c < '0' || c > '9') {
      return 0;
    }
  }
  return constrain((uint32_t)value.toInt(), 1u, 600u);
}

static int requestLocked(const char *method, const String &path, const String &body, String &response,
                         uint32_t *retryAfter) {
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
  static const char *wanted[] = {"Retry-After"};
  http.collectHeaders(wanted, 1);

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
  if (retryAfter) {
    *retryAfter = retryAfterSeconds(http);
  }
  MEMORY_CHECK("TLS session open");
  http.end();
  return status;
}

// Every call to Makapix goes through here, from the loader, the reporter and
// the display loop (registration), so they take turns.
static int request(const char *method, const String &path, const String &body, String &response,
                   uint32_t *retryAfter = nullptr) {
  xSemaphoreTake(lock, portMAX_DELAY);
  // Card access waits while a TLS request runs: a file being read in another
  // task was found closed under it whenever the two overlapped (see card.h).
  CardLock card;
  int status = requestLocked(method, path, body, response, retryAfter);
  xSemaphoreGive(lock);
  return status;
}

static void storeToken(const String &token) {
  apiToken = token;
  prefs.putString("api_token", apiToken);
}

// mbedTLS takes about 45 KB of internal RAM per connection when left to
// itself (the framework is built that way), and internal RAM is what Wi-Fi,
// task stacks and the display's DMA need. Its memory comes from PSRAM
// instead, falling back to internal RAM only if PSRAM is full. The hardware
// AES and SHA engines copy through their own internal buffers, so the TLS
// buffers do not need to be in DMA-capable memory.
static void *tlsCalloc(size_t count, size_t size) {
  void *memory = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return memory ? memory : heap_caps_calloc(count, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void tlsFree(void *memory) { heap_caps_free(memory); }

void begin() {
  mbedtls_platform_set_calloc_free(tlsCalloc, tlsFree);
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

QueryStatus nextPage(const channels::Channel &channel, PageState &page, std::vector<Artwork> &artworks,
                     String &problem) {
  artworks.clear();
  if (page.cursor.isEmpty()) {
    page.seed = esp_random() & 0x7fffffff; // new shuffle for each pass
  }

  JsonDocument body;
  body["request_type"] = "query_posts";
  body["channel"] = channels::makapixName(channel.kind);
  if (channel.kind == channels::Kind::Artist || channel.kind == channels::Kind::LikedBy) {
    body["user_handle"] = channel.value.c_str();
  } else if (channel.kind == channels::Kind::Hashtag) {
    body["hashtag"] = channel.value.c_str();
  }
  body["sort"] = "random";
  body["random_seed"] = page.seed;
  body["limit"] = PAGE_SIZE;
  if (!page.cursor.isEmpty()) {
    body["cursor"] = page.cursor;
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
  if (status == 400 || status == 404) {
    JsonDocument error;
    deserializeJson(error, response);
    problem = error["error"] | "Makapix refused the request";
    Serial.printf("Query refused: HTTP %d %s\n", status, problem.c_str());
    return QueryStatus::Refused;
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
  page.cursor = doc["has_more"].as<bool>() ? doc["next_cursor"].as<String>() : String();
  return QueryStatus::Ok;
}

bool getPost(uint32_t postId, Artwork &artwork, String &problem) {
  JsonDocument body;
  body["request_type"] = "get_post";
  body["post_id"] = postId;
  JsonArray fields = body["include_fields"].to<JsonArray>();
  fields.add("dwell_time_ms");
  fields.add("owner_handle");
  fields.add("width");
  fields.add("height");
  String payload;
  serializeJson(body, payload);
  String response;
  int status = request("POST", "/player/rpc", payload, response);
  JsonDocument doc;
  if (status <= 0 || deserializeJson(doc, response)) {
    problem = "Makapix could not be reached";
    return false;
  }
  if (status != 200 || !doc["success"].as<bool>()) {
    problem = doc["error"] | "Makapix would not supply it";
    return false;
  }
  JsonObject post = doc["post"];
  if ((post["width"] | 0) > PANEL_RES_X || (post["height"] | 0) > PANEL_RES_Y) {
    problem = "it is bigger than the panel";
    return false;
  }
  artwork.postId = post["post_id"] | postId;
  artwork.ownerHandle = post["owner_handle"] | "";
  artwork.gifUrl = gifUrlFor(post["art_url"].as<String>());
  artwork.dwellMs = post["dwell_time_ms"] | 30000;
  artwork.fromCache = false;
  return true;
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
  // A missing, hidden or deleted post (403, 404, 410), or a request Makapix
  // will never accept as it stands (400; 409, too many reactions on one post):
  // retrying cannot help.
  if (status == 400 || status == 403 || status == 404 || status == 409 || status == 410) {
    Serial.printf("Makapix refused: HTTP %d %s\n", status, response.c_str());
    return SendStatus::Gone;
  }
  return SendStatus::Failed;
}

SendStatus reportView(uint32_t postId, const String &isoTime, bool requested, const String &channel,
                      const String &hashtag, uint32_t &retryAfter) {
  JsonDocument body;
  body["post_id"] = postId;
  body["timestamp"] = isoTime;
  body["timezone"] = "";
  body["intent"] = requested ? "artwork" : "channel";
  body["play_order"] = 2; // random
  body["channel"] = channel.isEmpty() ? "all" : channel;
  if (!hashtag.isEmpty()) {
    body["channel_hashtag"] = hashtag;
  }
  String payload;
  serializeJson(body, payload);
  String response;
  return sendStatusFor(request("POST", "/player/events/view", payload, response, &retryAfter), response);
}

SendStatus setLiked(uint32_t postId, bool liked, uint32_t &retryAfter) {
  JsonDocument body;
  body["request_type"] = liked ? "submit_reaction" : "revoke_reaction";
  body["post_id"] = postId;
  body["emoji"] = HEART;
  String payload;
  serializeJson(body, payload);
  String response;
  int status = request("POST", "/player/rpc", payload, response, &retryAfter);
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

static bool readCertificates(const String &response, Certificates &out, String &problem) {
  JsonDocument filter;
  filter["ca_pem"] = true;
  filter["cert_pem"] = true;
  filter["key_pem"] = true;
  filter["broker"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) {
    problem = "Makapix sent certificates that could not be read";
    return false;
  }
  out.ca = doc["ca_pem"] | "";
  out.cert = doc["cert_pem"] | "";
  out.key = doc["key_pem"] | "";
  out.host = doc["broker"]["host"] | "makapix.club";
  out.port = doc["broker"]["port"] | 8883;
  if (out.ca.isEmpty() || out.cert.isEmpty() || out.key.isEmpty()) {
    problem = "Makapix sent no certificates";
    return false;
  }
  return true;
}

bool fetchCertificates(Certificates &out, String &problem) {
  if (playerKey.isEmpty()) {
    problem = "the panel is not linked to Makapix";
    return false;
  }
  String response;
  int status = request("GET", "/player/" + playerKey + "/credentials", "", response);
  if (status != 200) {
    problem = status == 429 ? "Makapix asked to wait before asking for certificates again"
                            : "could not get certificates from Makapix (HTTP " + String(status) + ")";
    return false;
  }
  return readCertificates(response, out, problem);
}

bool renewCertificates(Certificates &out, bool &notYet, String &problem) {
  notYet = false;
  String response;
  int status = request("POST", "/player/renew-cert", "", response);
  if (status == 400) {
    notYet = true;
    problem = "Makapix says the certificate is not due for renewal yet";
    return false;
  }
  if (status != 200) {
    problem = "could not renew the certificate (HTTP " + String(status) + ")";
    return false;
  }
  // renew-cert has no broker field; keep what the caller had.
  String host = out.host;
  uint16_t port = out.port;
  if (!readCertificates(response, out, problem)) {
    return false;
  }
  if (!host.isEmpty()) {
    out.host = host;
    out.port = port;
  }
  return true;
}

String key() { return playerKey; }

void forget() {
  prefs.remove("player_key");
  prefs.remove("api_token");
  playerKey = "";
  apiToken = "";
}

} // namespace makapix
