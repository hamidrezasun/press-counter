// gateway.ino - WiFi gateway of the press counter (ESP-12F on the v4 MAIN board).
//
// The STM32 does everything that matters locally (counting, display, relay, RFID, CAN).  This chip only:
//   * connects to WiFi, keeps the time (NTP) and gives it to the STM32,
//   * forwards the STM32's events to InfluxDB (v2) and / or MQTT, queued while the server is not reachable,
//   * serves the web page / JSON API and relays commands to the STM32,
//   * fetches firmware updates (itself over OTA, the STM32 through its ROM bootloader).
// Build:  Arduino IDE / arduino-cli, board "Generic ESP8266 Module", flash size 4MB (FS: 1MB), libraries ArduinoJson 6, PubSubClient.
// First flash through header H1 (GPIO0 low at reset); later updates over WiFi.  See docs/FIRMWARE.md.

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <sys/time.h>
#include <time.h>

#include "gw_config.h"
#include "gw_link.h"
#include "gw_settings.h"
#include "gw_state.h"
#include "gw_telemetry.h"
#include "gw_update.h"
#include "gw_web.h"

namespace {

DNSServer dns;
uint32_t g_lastOk = 0, g_apSince = 0, g_netAt = 0, g_timeAt = 0, g_statAt = 0, g_infoAt = 0;
bool g_otaStarted = false, g_mdnsStarted = false, g_linkWasAlive = false, g_timeSent = false;
char g_host[24] = "";

const char* deviceName() { return gwlink::identity().valid ? gwlink::identity().name : settings::cfg.device; }

void applyHostName() {
  char h[24];
  pc::sanitize(h, sizeof(h), deviceName());
  for (char* p = h; *p; p++) if (*p == ' ' || *p == '_') *p = '-';
  if (!strcmp(h, g_host)) return;
  pc::copyStr(g_host, h, sizeof(g_host));
  WiFi.hostname(g_host);
  if (g_otaStarted) ArduinoOTA.setHostname(g_host);
}

void sendNetStatus() {
  const fwupd::Info& u = fwupd::info();
  const String ip = gws::apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  gwlink::sendNet(ip.c_str(), gws::wifiUp, tele::serverOk(), gws::wifiUp ? WiFi.RSSI() : 0, gws::ntpOk, u.phase, u.pct);
}

void startWifi() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(gws::apMode ? WIFI_AP_STA : WIFI_STA);
  applyHostName();
  if (settings::hasWifi()) WiFi.begin(settings::cfg.wifiSsid, settings::cfg.wifiPass);
  g_lastOk = millis();
}

void startAp() {
  char ssid[28];
  snprintf(ssid, sizeof(ssid), "PressCounter-%06X", (unsigned)(ESP.getChipId() & 0xFFFFFF));
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, "presscounter");
  dns.start(53, "*", WiFi.softAPIP());
  gws::apMode = true;
  g_apSince = millis();
  gws::log("setup access point %s (password presscounter) on 192.168.4.1", ssid);
}

void stopAp() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  gws::apMode = false;
  gws::log("setup access point stopped");
}

void wifiService() {
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up != gws::wifiUp) gws::log(up ? "WiFi connected, ip %s" : "WiFi lost", WiFi.localIP().toString().c_str());
  gws::wifiUp = up;
  if (web::takeWifiChange()) {
    WiFi.disconnect();
    WiFi.begin(settings::cfg.wifiSsid, settings::cfg.wifiPass);
    g_lastOk = millis();
  }
  if (up) {
    g_lastOk = millis();
    if (gws::apMode && millis() - g_apSince > 180000UL) stopAp();       // the credentials work: close the setup network
    if (!g_otaStarted) {
      ArduinoOTA.setHostname(g_host);
      if (settings::cfg.otaPass[0]) ArduinoOTA.setPassword(settings::cfg.otaPass);
      ArduinoOTA.onProgress([](unsigned p, unsigned t) { (void)p; (void)t; });
      ArduinoOTA.begin();
      g_otaStarted = true;
    }
    if (!g_mdnsStarted && MDNS.begin(g_host)) { MDNS.addService("http", "tcp", 80); g_mdnsStarted = true; }
    if (g_mdnsStarted) MDNS.update();
    ArduinoOTA.handle();
  } else if (!gws::apMode && millis() - g_lastOk > 60000UL) {
    startAp();                                    // no credentials, wrong credentials or the router is gone: offer the setup page
  }
  if (gws::apMode) dns.processNextRequest();
}

void ntpService() {
  if (!gws::wifiUp) return;
  static bool configured = false;
  if (!configured) { configTime(0, 0, settings::cfg.ntpHost[0] ? settings::cfg.ntpHost : "pool.ntp.org", "time.google.com"); configured = true; }
  const bool ok = time(nullptr) > 1700000000L;
  if (ok && !gws::ntpOk) { gws::log("time synchronised"); g_timeSent = false; }
  gws::ntpOk = ok;
  if (ok && (!g_timeSent || millis() - g_timeAt > TIME_SYNC_MS)) {
    gwlink::sendTime();
    g_timeAt = millis();
    g_timeSent = true;
  }
}

void ledService() {
  const uint32_t t = millis();
  bool on;
  if (gws::wifiUp) on = true;
  else if (gws::apMode) on = (t / 1000) & 1;
  else on = (t / 200) & 1;
  digitalWrite(PIN_WIFI_LED, on ? LOW : HIGH);    // active low
}

}  // namespace

void setup() {
  pinMode(PIN_WIFI_LED, OUTPUT);
  digitalWrite(PIN_WIFI_LED, HIGH);
  settings::begin();
  gwlink::begin();
  gwlink::onEvent(tele::onEvent);
  tele::begin();
  fwupd::begin(sendNetStatus);
  web::begin();
  if (!settings::hasWifi()) { gws::apMode = true; startAp(); }
  else startWifi();
  gws::log("gateway %s started, reset reason: %s", GW_VERSION, ESP.getResetReason().c_str());
}

void loop() {
  gwlink::loop();
  web::loop();
  wifiService();
  ntpService();
  tele::loop();
  fwupd::loop();
  ledService();

  const uint32_t now = millis();
  const bool alive = gwlink::alive();
  if (alive && !g_linkWasAlive) {                 // the STM32 just appeared (power-up or after a flash): ask for what we need
    gwlink::send("R");
    gwlink::requestSettings();
    g_timeSent = false;
    g_netAt = 0;
    gws::log("counter link up");
  } else if (!alive && g_linkWasAlive) {
    gws::log("counter link lost");
  }
  g_linkWasAlive = alive;
  applyHostName();

  if (now - g_netAt >= NET_STATUS_MS) { g_netAt = now; sendNetStatus(); }
  if (gws::wifiUp && now - g_statAt >= 5000UL) { g_statAt = now; tele::publishStatus(); }
  if (gws::wifiUp && alive && (g_infoAt == 0 || now - g_infoAt >= 600000UL)) { g_infoAt = now; tele::publishDeviceInfo(); }
  if (gwlink::takeUpdateRequest()) fwupd::request(true);
}
