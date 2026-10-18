#include "network/live.h"

#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/x509_crt.h>
#include <time.h>

#include "app/commands.h"
#include "app/mode.h"
#include "app/night.h"
#include "app/rotation.h"
#include "app/status.h"
#include "app/telemetry.h"
#include "channels.h"
#include "config.h"
#include "live_rules.h"
#include "network/makapix.h"
#include "storage/card.h"

static const uint32_t TASK_STACK_BYTES = 6144; // peaked at 3.7 KB, handshakes included (TLS memory is in PSRAM)
static const BaseType_t TASK_CORE = 0;
static const uint16_t SOCKET_TIMEOUT_S = 10; // how long PubSubClient waits for the broker to answer
static const uint32_t CONNECTED_POLL_MS = 50;
static const uint32_t IDLE_POLL_MS = 1000;
static const uint16_t KEEP_ALIVE_S = 60;
static const uint32_t HEARTBEAT_MS = 60000;      // a status every minute, as the protocol asks
static const uint32_t CERT_RETRY_MS = 60000;     // after failing to get certificates
static const uint32_t DAY_MS = 24UL * 60 * 60 * 1000;
static const size_t MQTT_BUFFER_BYTES = 1024;    // commands are a few hundred bytes; a larger one (a playset) is dropped
static const time_t TRUSTED_AFTER = 1700000000;  // before this the clock is not set

enum class State { Off, Night, Waiting, Connecting, Connected, Retrying };

// PubSubClient waits for the broker by calling available() in a loop that
// never sleeps. On core 0 that starved the idle task, and the task watchdog
// restarted the board whenever the broker was slow to answer (2026-10-18).
// This wrapper sleeps a millisecond whenever nothing has arrived, and takes
// the card lock (storage/card.h) for each TLS operation on its own, so the
// lock is never held while waiting.
class LockedClient : public Client {
 public:
  explicit LockedClient(WiFiClientSecure &inner) : inner(inner) {}
  int connect(IPAddress ip, uint16_t port) override {
    CardLock card;
    return inner.connect(ip, port);
  }
  int connect(const char *host, uint16_t port) override {
    CardLock card;
    return inner.connect(host, port);
  }
  size_t write(uint8_t byte) override {
    CardLock card;
    return inner.write(byte);
  }
  size_t write(const uint8_t *buf, size_t size) override {
    CardLock card;
    return inner.write(buf, size);
  }
  int available() override {
    int ready;
    {
      CardLock card;
      ready = inner.available();
    }
    if (ready <= 0) {
      delay(1);
    }
    return ready;
  }
  int read() override {
    CardLock card;
    return inner.read();
  }
  int read(uint8_t *buf, size_t size) override {
    CardLock card;
    return inner.read(buf, size);
  }
  int peek() override {
    CardLock card;
    return inner.peek();
  }
  void flush() override {
    CardLock card;
    inner.flush();
  }
  void stop() override {
    CardLock card;
    inner.stop();
  }
  uint8_t connected() override {
    CardLock card;
    return inner.connected();
  }
  operator bool() override { return connected(); }

 private:
  WiFiClientSecure &inner;
};

static Preferences prefs;
static SemaphoreHandle_t lock; // guards the report fields below
static WiFiClientSecure tls;
static LockedClient lockedTls(tls);
static PubSubClient mqtt(lockedTls);
static makapix::Certificates certs;
static bool haveCerts = false;
static String certsFor; // the player key they were issued to

static bool enabled = true;
static State state = State::Off;
static String problem;
static uint32_t connectedAt = 0;
static uint32_t retryAt = 0;
static int failures = 0;          // connection attempts failed in a row
static uint32_t reconnects = 0;   // connections made since start-up, after the first
static uint32_t connections = 0;
static uint32_t commandsReceived = 0;
static String lastCommand;
static uint32_t lastCommandAt = 0;
static bool lastCommandHandled = false;
static String lastCommandId;
static uint32_t lastRenewAt = 0;
static bool renewedOnce = false;
static String certExpires; // "2029-10-11", from the certificate itself
static long certDaysLeft = 0;
static bool certDaysKnown = false;

static uint32_t publishedPost = UINT32_MAX;
static uint32_t lastStatusAt = 0;
static String topicCommand, topicStatus;

namespace {
struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};
} // namespace

static void setState(State next, const String &why = "") {
  Guard guard;
  state = next;
  problem = why;
}

// ---- certificates ----------------------------------------------------------------

// The TLS client keeps pointers to the PEM texts and reads them at every
// handshake. They live in PSRAM, so about 4.5 KB of internal RAM stays free.
static char *pemCa = nullptr, *pemCert = nullptr, *pemKey = nullptr;

static void readExpiry() {
  mbedtls_x509_crt crt;
  mbedtls_x509_crt_init(&crt);
  bool parsed = pemCert && mbedtls_x509_crt_parse(&crt, (const unsigned char *)pemCert, strlen(pemCert) + 1) == 0;
  Guard guard;
  if (parsed) {
    char date[11];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", crt.valid_to.year, crt.valid_to.mon, crt.valid_to.day);
    certExpires = date;
    time_t now = time(nullptr);
    certDaysKnown = now >= TRUSTED_AFTER;
    certDaysLeft = certDaysKnown ? live::daysUntil(crt.valid_to.year, crt.valid_to.mon, crt.valid_to.day, now) : 0;
  } else {
    certExpires = "";
    certDaysKnown = false;
  }
  mbedtls_x509_crt_free(&crt);
}

static void saveCerts() {
  prefs.putString("ca", certs.ca);
  prefs.putString("cert", certs.cert);
  prefs.putString("key", certs.key);
  prefs.putString("host", certs.host);
  prefs.putUShort("port", certs.port);
  prefs.putString("for", makapix::key());
  certsFor = makapix::key();
}

static bool loadCerts() {
  certs.ca = prefs.getString("ca", "");
  certs.cert = prefs.getString("cert", "");
  certs.key = prefs.getString("key", "");
  certs.host = prefs.getString("host", "makapix.club");
  certs.port = prefs.getUShort("port", 8883);
  certsFor = prefs.getString("for", "");
  return !certs.ca.isEmpty() && !certs.cert.isEmpty() && !certs.key.isEmpty();
}


static char *psramCopy(char *old, const String &text) {
  free(old);
  char *copy = (char *)ps_malloc(text.length() + 1);
  if (copy) {
    memcpy(copy, text.c_str(), text.length() + 1);
  }
  return copy;
}

static void useCerts() {
  pemCa = psramCopy(pemCa, certs.ca);
  pemCert = psramCopy(pemCert, certs.cert);
  pemKey = psramCopy(pemKey, certs.key);
  tls.setCACert(pemCa);
  tls.setCertificate(pemCert);
  tls.setPrivateKey(pemKey);
  mqtt.setServer(certs.host.c_str(), certs.port);
  readExpiry();
  certs.ca = certs.cert = certs.key = ""; // the PSRAM copies are the ones in use
  haveCerts = true;
}

static bool getCerts() {
  String why;
  makapix::Certificates fresh;
  if (!makapix::fetchCertificates(fresh, why)) {
    setState(State::Retrying, why);
    retryAt = millis() + CERT_RETRY_MS;
    return false;
  }
  certs = fresh; // make-before-break: only replaced once the new set is complete
  saveCerts();
  useCerts();
  Serial.println("Live: certificates fetched");
  return true;
}

static void renewCerts() {
  lastRenewAt = millis();
  renewedOnce = true;
  makapix::Certificates fresh = certs;
  bool notYet = false;
  String why;
  if (!makapix::renewCertificates(fresh, notYet, why)) {
    Serial.printf("Live: %s\n", why.c_str());
    if (notYet && failures >= live::RENEW_AFTER_FAILURES) {
      getCerts(); // not due, yet refused: perhaps ours are stale; fetch the current ones
    }
    return;
  }
  certs = fresh;
  saveCerts();
  useCerts();
  failures = 0;
  Serial.printf("Live: certificate renewed, valid until %s\n", certExpires.c_str());
}

// ---- status and commands ------------------------------------------------------------

static void publishStatus(bool online) {
  PlayerStatus now = statusGet();
  JsonDocument doc;
  doc["player_key"] = makapix::key();
  doc["status"] = online ? "online" : "offline";
  if (now.hasArtwork) {
    doc["current_post_id"] = now.postId;
  } else {
    doc["current_post_id"] = nullptr;
  }
  doc["firmware_version"] = FIRMWARE_VERSION;
  String payload;
  serializeJson(doc, payload);
  mqtt.publish(topicStatus.c_str(), payload.c_str());
  publishedPost = now.hasArtwork ? now.postId : 0;
  lastStatusAt = millis();
}

// Runs inside mqtt.loop(): only queues work for the display loop, or changes
// settings that are safe from any task.
static bool handleCommand(const String &type, JsonObject payload) {
  if (type == "swap_next") {
    return commandSend(CommandType::Next);
  }
  if (type == "swap_back") {
    return commandSend(CommandType::Back);
  }
  if (type == "show_artwork") {
    uint32_t postId = payload["post_id"] | 0;
    return postId && commandSend(CommandType::ShowArtwork, postId);
  }
  if (type == "play_channel") {
    channels::Channel channel;
    if (!channels::fromMakapix(payload["channel_name"] | "", payload["user_handle"] | "", payload["hashtag"] | "",
                               channel)) {
      return false;
    }
    String error;
    if (!rotationSetChannelOverride(channels::format(channel).c_str(), "makapix.club", error)) {
      return false;
    }
    // Asked for art: leave sound mode, and start the channel now.
    modeSet(false, "", error);
    return commandSend(CommandType::Reload);
  }
  return false; // play_playset and anything newer: not supported yet
}

static void onMessage(char *topic, uint8_t *data, unsigned int length) {
  if (topicCommand != topic) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, data, length)) {
    Serial.println("Live: a command that is not JSON");
    return;
  }
  String id = doc["command_id"] | "";
  String type = doc["command_type"] | "";
  if (!id.isEmpty() && id == lastCommandId) {
    return; // delivered twice (QoS 1 allows it)
  }
  bool handled = handleCommand(type, doc["payload"].as<JsonObject>());
  Serial.printf("Live: command %s%s\n", type.c_str(), handled ? "" : " (not acted on)");
  Guard guard;
  lastCommandId = id;
  commandsReceived++;
  lastCommand = type;
  lastCommandAt = millis();
  lastCommandHandled = handled;
}

// ---- the connection -------------------------------------------------------------------

static bool connectNow() {
  setState(State::Connecting);
  String key = makapix::key();
  topicCommand = "makapix/player/" + key + "/command";
  topicStatus = "makapix/player/" + key + "/status";
  JsonDocument will;
  will["player_key"] = key;
  will["status"] = "offline";
  String willPayload;
  serializeJson(will, willPayload);

  bool ok = mqtt.connect(key.c_str(), key.c_str(), "", topicStatus.c_str(), 1, false, willPayload.c_str()) &&
            mqtt.subscribe(topicCommand.c_str(), 1);
  if (ok) {
    publishStatus(true);
  }
  if (!ok) {
    char tlsError[100] = "";
    tls.lastError(tlsError, sizeof(tlsError));
    failures++;
    uint32_t wait = live::retryDelaySeconds(failures);
    retryAt = millis() + wait * 1000;
    String why = "could not connect (MQTT state " + String(mqtt.state()) + (tlsError[0] ? String(", ") + tlsError : String()) + ")";
    Serial.printf("Live: %s; trying again in %u s\n", why.c_str(), (unsigned)wait);
    setState(State::Retrying, why);
    mqtt.disconnect();
    lockedTls.stop();
    return false;
  }
  failures = 0;
  {
    Guard guard;
    reconnects += connections > 0 ? 1 : 0;
    connections++;
    connectedAt = millis();
  }
  setState(State::Connected);
  Serial.println("Live: connected to makapix.club");
  return true;
}

static void sayGoodbye() {
  if (mqtt.connected()) {
    publishStatus(false);
    mqtt.disconnect();
  }
  lockedTls.stop();
}

static void step() {
  bool on;
  {
    Guard guard;
    on = enabled;
  }
  // While the night schedule has the panel off, it says goodbye and is
  // offline on makapix.club until morning, or until someone wakes it here.
  bool night = nightNow().off;
  if (!on || night || WiFi.status() != WL_CONNECTED || !makapix::isRegistered()) {
    if (mqtt.connected()) {
      Serial.println(night ? "Live: off for the night" : "Live: disconnecting");
      sayGoodbye();
    }
    failures = 0; // a fresh start when it may connect again
    retryAt = 0;
    setState(!on ? State::Off : night ? State::Night : State::Waiting,
             !on || night ? "" : WiFi.status() != WL_CONNECTED ? "waiting for Wi-Fi" : "waiting for the panel to be linked");
    return;
  }
  if (mqtt.connected()) {
    mqtt.loop();
    PlayerStatus now = statusGet();
    uint32_t post = now.hasArtwork ? now.postId : 0;
    if (mqtt.connected() && (post != publishedPost || millis() - lastStatusAt >= HEARTBEAT_MS)) {
      publishStatus(true);
    }
    if (mqtt.connected()) {
      return;
    }
    Serial.println("Live: connection lost");
    lockedTls.stop();
    failures++;
    retryAt = millis() + live::retryDelaySeconds(failures) * 1000;
    setState(State::Retrying, "connection lost");
    return;
  }
  if ((int32_t)(millis() - retryAt) < 0) {
    return;
  }
  if (haveCerts && certsFor != makapix::key()) {
    haveCerts = false; // linked again since: a new identity needs new certificates
  }
  if (!haveCerts && !getCerts()) {
    return;
  }
  readExpiry(); // the clock may have been set since
  if (live::shouldRenew(certDaysLeft, certDaysKnown, failures, renewedOnce && millis() - lastRenewAt < DAY_MS)) {
    renewCerts();
  }
  connectNow();
}

static void liveTask(void *) {
  for (;;) {
    step();
    vTaskDelay(pdMS_TO_TICKS(mqtt.connected() ? CONNECTED_POLL_MS : IDLE_POLL_MS));
  }
}

void liveBegin() {
  lock = xSemaphoreCreateMutex();
  prefs.begin("live", false);
  enabled = prefs.getBool("enabled", true);
  mqtt.setBufferSize(MQTT_BUFFER_BYTES);
  mqtt.setKeepAlive(KEEP_ALIVE_S);
  mqtt.setSocketTimeout(SOCKET_TIMEOUT_S);
  mqtt.setCallback(onMessage);
  tls.setHandshakeTimeout(20);
  if (loadCerts()) {
    useCerts();
  }
  TaskHandle_t task = nullptr;
  xTaskCreatePinnedToCore(liveTask, "live", TASK_STACK_BYTES, nullptr, 1, &task, TASK_CORE);
  telemetryRegisterTask("live", task, TASK_STACK_BYTES);
}

bool liveEnabled() {
  Guard guard;
  return enabled;
}

void liveSetEnabled(bool on) {
  Guard guard;
  enabled = on;
  prefs.putBool("enabled", on);
  retryAt = 0;
  failures = 0;
}

static const char *stateName(State value) {
  switch (value) {
    case State::Night:
      return "night";
    case State::Waiting:
      return "waiting";
    case State::Connecting:
      return "connecting";
    case State::Connected:
      return "connected";
    case State::Retrying:
      return "retrying";
    default:
      return "off";
  }
}

void liveReport(JsonObject out) {
  Guard guard;
  out["enabled"] = enabled;
  out["state"] = stateName(state);
  if (state == State::Connected) {
    out["connectedSeconds"] = (millis() - connectedAt) / 1000;
  }
  out["reconnects"] = reconnects;
  out["commandsReceived"] = commandsReceived;
  if (lastCommand.isEmpty()) {
    out["lastCommand"] = nullptr;
  } else {
    JsonObject last = out["lastCommand"].to<JsonObject>();
    last["type"] = lastCommand;
    last["secondsAgo"] = (millis() - lastCommandAt) / 1000;
    last["actedOn"] = lastCommandHandled;
  }
  if (certExpires.isEmpty()) {
    out["certificateExpires"] = nullptr;
  } else {
    out["certificateExpires"] = certExpires;
  }
  if (!problem.isEmpty()) {
    out["problem"] = problem;
  }
}
