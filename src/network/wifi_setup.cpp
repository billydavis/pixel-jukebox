#include "network/wifi_setup.h"

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <vector>

#include "display/display.h"
#include "html_escape.h"
#include "network/web_assets.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

// Not 192.168.4.1, the usual default, which clashes with home networks on 192.168.4.x.
static const IPAddress SETUP_IP(10, 10, 10, 1);
static const IPAddress SETUP_MASK(255, 255, 255, 0);
static const uint32_t SETUP_RETRY_MS = 3 * 60 * 1000; // then try the known network again
static const uint32_t JOIN_TEST_MS = 15000;           // how long a new network gets to accept us
static const uint32_t RESTART_DELAY_MS = 6000;        // lets the phone fetch the result page
static const size_t MAX_NETWORKS = 12;

struct Network {
  String ssid;
  int32_t rssi;
  bool secured;
};

static Preferences prefs;
static std::vector<Network> networks;

void wifiBegin() { prefs.begin("jukebox", false); }

static void credentials(String &ssid, String &password) {
  if (prefs.isKey("wifi_ssid")) {
    ssid = prefs.getString("wifi_ssid", "");
    password = prefs.getString("wifi_pass", "");
    return;
  }
  ssid = WIFI_SSID;
  password = WIFI_PASSWORD;
}

bool wifiHasCredentials() {
  String ssid, password;
  credentials(ssid, password);
  return !ssid.isEmpty();
}

bool wifiConnect(uint32_t timeoutMs) {
  String ssid, password;
  credentials(ssid, password);
  if (ssid.isEmpty()) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      return false;
    }
    delay(250);
  }
  return true;
}

void wifiRequestSetup() {
  prefs.putBool("wifi_setup", true);
  ESP.restart();
}

bool wifiSetupRequested() {
  if (!prefs.getBool("wifi_setup", false)) {
    return false;
  }
  prefs.remove("wifi_setup");
  return true;
}

// Network names come from whoever is nearby, so they are never trusted as HTML.
static String escapeHtml(const String &text) { return htmlEscape(text.c_str()).c_str(); }

// Strongest first, one entry per name. The radio only sees 2.4 GHz networks.
static void scanNetworks() {
  networks.clear();
  int found = WiFi.scanNetworks();
  for (int i = 0; i < found; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      continue; // hidden
    }
    bool known = false;
    for (Network &network : networks) {
      if (network.ssid == ssid) {
        known = true;
        network.rssi = max(network.rssi, WiFi.RSSI(i));
      }
    }
    if (!known) {
      networks.push_back({ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
    }
  }
  WiFi.scanDelete();

  std::sort(networks.begin(), networks.end(),
            [](const Network &a, const Network &b) { return a.rssi > b.rssi; });
  if (networks.size() > MAX_NETWORKS) {
    networks.resize(MAX_NETWORKS);
  }
}

static String networkList() {
  if (networks.empty()) {
    return "<p class=\"note\">No networks found. Scan again, or type the name below.</p>";
  }
  String html;
  for (const Network &network : networks) {
    String name = escapeHtml(network.ssid);
    const char *signal = network.rssi > -60 ? "Strong" : network.rssi > -72 ? "Good" : "Weak";
    html += "<button type=\"button\" class=\"net\" data-ssid=\"" + name + "\"><span>" + name +
            "</span><small>" + signal + (network.secured ? ", password" : ", open") +
            "</small></button>";
  }
  return html;
}

static String setupPage(const String &alert, const String &ssid) {
  String page = WEB_SETUP_HTML;
  page.replace("{{ALERT}}",
               alert.isEmpty() ? "" : "<p class=\"alert\" role=\"alert\">" + alert + "</p>");
  page.replace("{{NETWORKS}}", networkList());
  page.replace("{{SSID}}", escapeHtml(ssid));
  return page;
}

static void showSetupScreen() {
  display->clearScreen();
  display->setTextSize(1);
  display->setTextWrap(false);
  display->setTextColor(display->color565(255, 200, 0));
  display->setCursor(2, 2);
  display->print("WiFi setup");
  display->setTextColor(display->color565(150, 150, 150));
  display->setCursor(2, 16);
  display->print("On a phone");
  display->setCursor(2, 26);
  display->print("join WiFi:");
  display->setTextColor(display->color565(255, 255, 255));
  display->setCursor(2, 40);
  display->print("Pixel-");
  display->setCursor(2, 50);
  display->print("Jukebox");
}

// Setup is a small state machine so the web server keeps answering while a
// new network is being tried.
enum class SetupState { Waiting, Testing, Joined };

static SetupState setupState = SetupState::Waiting;
static uint32_t stateSince = 0;
static String pendingSsid;
static String pendingPassword;
static String formAlert;
static String formSsid;

static void redirect(WebServer &server, const char *path) {
  server.sendHeader("Location", "http://" + SETUP_IP.toString() + path);
  server.send(302, "text/plain", "");
}

static String filledPage(const char *page, const String &ssid) {
  String html = page;
  html.replace("{{SSID}}", escapeHtml(ssid));
  return html;
}

void wifiSetupRun(bool joinFailed) {
  static DNSServer dns;
  static WebServer server(80);

  String knownSsid, knownPassword;
  credentials(knownSsid, knownPassword);
  if (joinFailed) {
    // The name is filled in, so only the password needs retyping.
    formAlert = "Couldn't join \"" + escapeHtml(knownSsid) + "\". Check the password and try again.";
    formSsid = knownSsid;
  }

  Serial.println("Starting Wi-Fi setup hotspot " WIFI_SETUP_SSID);
  showSetupScreen();

  // AP for the phone, plus station mode to scan and to try the chosen network.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(SETUP_IP, SETUP_IP, SETUP_MASK);
  WiFi.softAP(WIFI_SETUP_SSID);
  scanNetworks();

  // Answering every DNS lookup with our own address is what makes phones
  // open the setup page automatically.
  dns.start(53, "*", SETUP_IP);

  server.on("/", HTTP_GET, []() {
    if (setupState != SetupState::Waiting) {
      redirect(server, "/result");
      return;
    }
    if (server.hasArg("scan")) {
      scanNetworks();
    }
    server.send(200, "text/html", setupPage(formAlert, formSsid));
  });

  // Tries the network before saving anything; /result reports how it went.
  server.on("/save", HTTP_POST, []() {
    if (setupState != SetupState::Waiting) {
      redirect(server, "/result");
      return;
    }
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    ssid.trim();
    if (ssid.isEmpty() || ssid.length() > 32) {
      server.send(400, "text/html", setupPage("Enter the network name.", ssid));
      return;
    }
    if (!password.isEmpty() && (password.length() < 8 || password.length() > 63)) {
      server.send(400, "text/html", setupPage("Wi-Fi passwords are 8 to 63 characters.", ssid));
      return;
    }

    pendingSsid = ssid;
    pendingPassword = password;
    setupState = SetupState::Testing;
    stateSince = millis();
    Serial.printf("Trying Wi-Fi network %s\n", ssid.c_str());
    showMessage("WiFi", "checking\npassword", display->color565(255, 200, 0));
    server.send(200, "text/html", filledPage(WEB_SETUP_CHECK_HTML, ssid));
    WiFi.begin(ssid.c_str(), password.c_str());
  });

  server.on("/result", HTTP_GET, []() {
    switch (setupState) {
      case SetupState::Testing:
        server.send(200, "text/html", filledPage(WEB_SETUP_CHECK_HTML, pendingSsid));
        break;
      case SetupState::Joined:
        server.send(200, "text/html", filledPage(WEB_SETUP_DONE_HTML, pendingSsid));
        break;
      default:
        redirect(server, "/");
    }
  });

  server.on("/setup.css", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "max-age=600");
    server.send_P(200, "text/css", WEB_SETUP_CSS);
  });

  // Phones probe assorted addresses to detect a sign-in page; send them all here.
  server.onNotFound([]() { redirect(server, "/"); });
  server.begin();

  uint32_t lastClientAt = millis();
  for (;;) {
    dns.processNextRequest();
    server.handleClient();
    delay(2);

    if (setupState == SetupState::Testing) {
      if (WiFi.status() == WL_CONNECTED) {
        prefs.putString("wifi_ssid", pendingSsid);
        prefs.putString("wifi_pass", pendingPassword);
        Serial.printf("Joined %s; details saved\n", pendingSsid.c_str());
        showMessage("WiFi", "connected", display->color565(0, 255, 0));
        setupState = SetupState::Joined;
        stateSince = millis();
      } else if (millis() - stateSince > JOIN_TEST_MS) {
        Serial.printf("Could not join %s; nothing saved\n", pendingSsid.c_str());
        WiFi.disconnect(); // stops the retries; the hotspot stays up
        formAlert = "Couldn't join \"" + escapeHtml(pendingSsid) +
                    "\". Check the password and try again.";
        formSsid = pendingSsid;
        showSetupScreen();
        setupState = SetupState::Waiting;
      }
    } else if (setupState == SetupState::Joined) {
      if (millis() - stateSince > RESTART_DELAY_MS) {
        ESP.restart();
      }
    }

    if (WiFi.softAPgetStationNum() > 0 || setupState != SetupState::Waiting) {
      lastClientAt = millis();
    } else if (!knownSsid.isEmpty() && millis() - lastClientAt > SETUP_RETRY_MS) {
      Serial.println("Nobody used setup; trying the known network again");
      ESP.restart();
    }
  }
}
