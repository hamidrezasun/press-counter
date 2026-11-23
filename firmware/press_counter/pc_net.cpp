#include "pc_net.h"
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "pc_log.h"
#include "pc_session.h"
#include "pc_store.h"

namespace {
DNSServer g_dns;
bool g_up = false, g_ap = false, g_otaStarted = false, g_mdnsStarted = false, g_ntpConfigured = false, g_reconnect = false;
uint32_t g_lastOk = 0, g_apSince = 0;
char g_apSsid[28] = "", g_ip[16] = "", g_host[NAME_LEN + 2] = "";

void makeHostName() {
  char h[NAME_LEN + 2];
  pc::sanitize(h, sizeof(h), store::cfg.name);
  for (char* p = h; *p; p++) if (!isalnum((unsigned char)*p)) *p = '-';
  pc::copyStr(g_host, h, sizeof(g_host));
}

void startStation() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(g_host);
  WiFi.mode(g_ap ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);                                  // lowest latency for the web page / MQTT; the board is mains powered
  if (store::net.wifiSsid[0]) WiFi.begin(store::net.wifiSsid, store::net.wifiPass);
  g_lastOk = millis();
}

void stopAp() {
  g_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  g_ap = false;
  plog::log("setup access point closed");
}
}  // namespace

namespace net {

void begin() {
  makeHostName();
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(g_apSsid, sizeof(g_apSsid), "PressCounter-%06X", (unsigned)((mac >> 24) & 0xFFFFFF));
  if (!store::net.wifiSsid[0]) startSetupAp();
  else startStation();
}

void startSetupAp() {
  if (g_ap) { g_apSince = millis(); return; }
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(g_apSsid, "presscounter");
  g_dns.start(53, "*", WiFi.softAPIP());
  g_ap = true;
  g_apSince = millis();
  session::banner("WIFI SETUP OPEN", 8000);
  plog::log("setup access point %s (password presscounter) on 192.168.4.1", g_apSsid);
  if (store::net.wifiSsid[0]) WiFi.begin(store::net.wifiSsid, store::net.wifiPass);
}

void reconnect() { g_reconnect = true; }

void loop() {
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up != g_up) {
    if (up) { pc::copyStr(g_ip, WiFi.localIP().toString().c_str(), sizeof(g_ip)); plog::log("WiFi connected, ip %s", g_ip); }
    else { g_ip[0] = '\0'; plog::log("WiFi lost"); }
  }
  g_up = up;
  if (g_reconnect) {
    g_reconnect = false;
    WiFi.disconnect();
    if (store::net.wifiSsid[0]) WiFi.begin(store::net.wifiSsid, store::net.wifiPass);
    g_lastOk = millis();
  }
  if (up) {
    g_lastOk = millis();
    if (g_ap && millis() - g_apSince > 180000UL) stopAp();       // the credentials work: close the setup network after 3 minutes
    if (!g_ntpConfigured) {
      configTime(0, 0, store::net.ntpHost[0] ? store::net.ntpHost : "pool.ntp.org", "time.google.com");
      g_ntpConfigured = true;
    }
    if (!g_otaStarted) {
      ArduinoOTA.setHostname(g_host);
      if (store::net.otaPass[0]) ArduinoOTA.setPassword(store::net.otaPass);
      ArduinoOTA.onStart([]() { session::setRelay(false, pc::RR_REMOTE); store::lifetimeFlush(true); });     // machine off before flashing
      ArduinoOTA.begin();
      g_otaStarted = true;
    }
    if (!g_mdnsStarted && MDNS.begin(g_host)) { MDNS.addService("http", "tcp", 80); g_mdnsStarted = true; }
    ArduinoOTA.handle();
  } else if (!g_ap && millis() - g_lastOk > 120000UL) {
    startSetupAp();                                                // wrong credentials or the router is gone for 2 min: offer the setup page
  }
  if (g_ap) {
    g_dns.processNextRequest();
    if (!store::net.wifiSsid[0]) g_apSince = millis();             // never close it while there is nothing else
  }
}

bool wifiUp() { return g_up; }
bool apMode() { return g_ap; }
const char* apSsid() { return g_apSsid; }
const char* ip() { return g_up ? g_ip : (g_ap ? "192.168.4.1" : ""); }
int rssi() { return g_up ? WiFi.RSSI() : 0; }
const char* hostName() { return g_host; }

}  // namespace net
