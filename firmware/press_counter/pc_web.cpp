#include "pc_web.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>
#include "pc_config.h"
#include "pc_buttons.h"
#include "pc_errors.h"
#include "pc_events.h"
#include "pc_io.h"
#include "pc_log.h"
#include "pc_mqtt.h"
#include "pc_net.h"
#include "pc_nfc.h"
#include "pc_outputs.h"
#include "pc_page.h"
#include "pc_sense.h"
#include "pc_press.h"
#include "pc_session.h"
#include "pc_store.h"
#include "pc_temp.h"
#include "pc_update.h"

namespace web {
namespace {

WebServer srv(80);
uint32_t g_restartAt = 0;
bool g_serving = false;

void cors() {
  srv.sendHeader("Access-Control-Allow-Origin", "*");
  srv.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  srv.sendHeader("Access-Control-Allow-Headers", "*");
  srv.sendHeader("Cache-Control", "no-store");
}
void sendText(int code, const char* text) { cors(); srv.send(code, "text/plain", text); }
void sendDoc(const JsonDocument& d, int code = 200) {
  String out;
  serializeJson(d, out);
  cors();
  srv.send(code, "application/json", out);
}

// "admin" + admin password guards everything that changes something.  Without an admin password the device is open (LAN only!).
bool authed() {
  const char* p = store::net.adminPass;
  if (!p[0]) return true;
  if (srv.authenticate("admin", p)) return true;
  srv.requestAuthentication(BASIC_AUTH, "press counter");
  return false;
}
#define NEED_AUTH() do { if (!authed()) return; } while (0)

// ── state ──────────────────────────────────────────────────────────────────────────────────────────────────────────
void handleState() {
  const fwupd::Info& u = fwupd::info();
  DynamicJsonDocument d(2048);
  d["device"] = store::cfg.name;
  d["version"] = PC_FW_VERSION_STR;
  d["uptime"] = millis() / 1000UL;
  d["heap"] = ESP.getFreeHeap();
  d["time"] = clk::sec();
  d["tag"] = nfc::lastTag();
  JsonObject n = d.createNestedObject("net");
  n["wifi"] = net::wifiUp();
  n["ap"] = net::apMode();
  n["apSsid"] = net::apSsid();
  n["ssid"] = WiFi.SSID();
  n["ip"] = net::ip();
  n["rssi"] = net::rssi();
  n["mac"] = WiFi.macAddress();
  n["ntp"] = clk::valid();
  n["mqtt"] = mqtt::connected();
  n["mqttConfigured"] = mqtt::configured();
  JsonObject m = d.createNestedObject("m");
  m["relay"] = session::relayOn();
  m["count"] = press::count();
  m["lifetime"] = store::lifetime();
  m["lastDurMs"] = press::lastDurationMs();
  m["inPress"] = press::active();
  const uint32_t since = press::sinceLastStrokeMs();
  m["lastAgoS"] = since == 0xFFFFFFFFUL ? -1L : (long)(since / 1000UL);
  const int16_t t = temp::tempC10();
  if (t != INT16_MIN) m["tempC"] = t / 10.0f; else m["tempC"] = nullptr;
  m["err"] = err::top();
  m["errText"] = pc::errorText(err::top());
  m["errMask"] = err::mask();
  m["operator"] = session::operatorId();
  m["die"] = session::dieId();
  m["dieTest"] = session::dieTest();
  m["sessionSec"] = session::sessionSeconds();
  m["calls"] = session::callMask();
  m["queued"] = events::pending();
  m["dropped"] = events::dropped();
  m["published"] = events::published();
  m["ledAlert"] = io::alertLit();
  m["btnSup"] = buttons::down(1);
  m["btnNet"] = buttons::down(2);
  m["nfc"] = nfc::present();
  m["writePending"] = nfc::writePending();
  m["banner"] = session::bannerText();
  m["outs"] = outputs::mask();
  m["v12"] = sense::v12mV() / 1000.0f;
  m["aux"] = sense::auxmV() / 1000.0f;
  JsonObject up = d.createNestedObject("update");
  up["phase"] = u.phase;
  up["pct"] = u.pct;
  up["msg"] = u.msg;
  up["latest"] = u.latest;
  up["isNew"] = u.isNew;
  up["configured"] = store::net.updateUrl[0] != 0;
  up["busy"] = fwupd::busy();
  sendDoc(d);
}

// ── settings ───────────────────────────────────────────────────────────────────────────────────────────────────────
void handleGetSettings() {
  NEED_AUTH();
  const NetSettings& c = store::net;
  DynamicJsonDocument d(2048);
  JsonObject g = d.createNestedObject("net");
  g["mqttHost"] = c.mqttHost;
  g["mqttPort"] = c.mqttPort;
  g["mqttTopic"] = c.mqttTopic;
  g["mqttClient"] = c.mqttClient;
  g["mqttUser"] = c.mqttUser;
  g["mqttPassSet"] = c.mqttPass[0] != 0;
  g["updateUrl"] = c.updateUrl;
  g["updateTokenSet"] = c.updateToken[0] != 0;
  g["autoUpdate"] = c.autoUpdate;
  g["checkMinutes"] = c.checkMinutes;
  g["ntpHost"] = c.ntpHost;
  g["adminPassSet"] = c.adminPass[0] != 0;
  g["otaPassSet"] = c.otaPass[0] != 0;
  g["wifiSsid"] = c.wifiSsid;
  JsonObject m = d.createNestedObject("machine");
  char v[32];
  for (int i = 0; i < store::paramCount(); i++)
    if (store::getParam(store::paramKey(i), v, sizeof(v))) m[store::paramKey(i)] = v;
  sendDoc(d);
}

// JSON body: {"net":{...},"machine":{"minpulse":"20",...}}.  Secrets are only replaced by a non-empty value.
void handlePostSettings() {
  NEED_AUTH();
  DynamicJsonDocument d(2048);
  if (deserializeJson(d, srv.arg("plain"))) { sendText(400, "bad JSON"); return; }
  NetSettings& c = store::net;
  JsonObject g = d["net"];
  auto s = [&](const char* k, char* dst, size_t n, bool secret = false) {
    if (!g.containsKey(k)) return;
    const char* v = g[k] | "";
    if (secret && !v[0]) return;
    pc::copyStr(dst, v, n);
  };
  s("mqttHost", c.mqttHost, sizeof(c.mqttHost));
  s("mqttTopic", c.mqttTopic, sizeof(c.mqttTopic));
  s("mqttClient", c.mqttClient, sizeof(c.mqttClient));
  s("mqttUser", c.mqttUser, sizeof(c.mqttUser));
  s("mqttPass", c.mqttPass, sizeof(c.mqttPass), true);
  s("updateUrl", c.updateUrl, sizeof(c.updateUrl));
  s("updateToken", c.updateToken, sizeof(c.updateToken), true);
  s("ntpHost", c.ntpHost, sizeof(c.ntpHost));
  s("adminPass", c.adminPass, sizeof(c.adminPass), true);
  s("otaPass", c.otaPass, sizeof(c.otaPass), true);
  if (g.containsKey("mqttPort")) c.mqttPort = (uint16_t)constrain((int)(g["mqttPort"] | 1883), 1, 65535);
  if (g.containsKey("autoUpdate")) c.autoUpdate = (g["autoUpdate"] | 0) ? 1 : 0;
  if (g.containsKey("checkMinutes")) c.checkMinutes = (uint16_t)constrain((int)(g["checkMinutes"] | 360), 5, 10080);

  String bad;
  JsonObject m = d["machine"];
  for (JsonPair kv : m) {
    char val[40];
    if (kv.value().is<const char*>()) pc::copyStr(val, kv.value().as<const char*>(), sizeof(val));
    else snprintf(val, sizeof(val), "%ld", (long)kv.value().as<long>());
    if (!store::setParam(kv.key().c_str(), val)) { bad += bad.length() ? ", " : ""; bad += kv.key().c_str(); }
  }
  if (!store::saveNet() || !store::save()) { sendText(500, "Failed to save settings"); return; }
  mqtt::reconfigure();
  if (bad.length()) { sendText(400, ("Saved, but these values were rejected (out of range): " + bad).c_str()); return; }
  sendText(200, "saved");
}

// ── commands ───────────────────────────────────────────────────────────────────────────────────────────────────────
void handleCmd() {
  NEED_AUTH();
  const String cmd = srv.arg("cmd"), arg = srv.arg("arg");
  const int a = arg.toInt();
  if (cmd == "relay") {
    if (arg == "0") session::remoteCommand(pc::CMD_RELAY_OFF, 0, pc::RR_WEB);
    else if (arg == "1") session::remoteCommand(pc::CMD_RELAY_ON, 0, pc::RR_WEB);
    else session::remoteCommand(pc::CMD_RELAY_TOGGLE, 0, pc::RR_WEB);
  } else if (cmd == "out") {                                       // arg = n:s  (n = 1..4, s = 0 off / 1 on / 2 toggle)
    const int colon = arg.indexOf(':');
    const int n = arg.toInt(), st = colon > 0 ? arg.substring(colon + 1).toInt() : 2;
    if (!outputs::command((uint8_t)n, (uint8_t)st, pc::RR_WEB)) {
      sendText(403, "Refused: the output is not in manual mode, or switching on needs 'Allow remote start' (off is always allowed)");
      return;
    }
  } else if (cmd == "reset") session::remoteCommand(pc::CMD_RESET_CNT, 0, pc::RR_WEB);
  else if (cmd == "buzz") session::remoteCommand(pc::CMD_BUZZ, arg == "success" ? 0 : arg == "warning" ? 1 : 2, pc::RR_WEB);
  else if (cmd == "call") session::remoteCommand(pc::CMD_CALL, (uint8_t)a, pc::RR_WEB);
  else if (cmd == "ack") session::remoteCommand(pc::CMD_ACK_CALL, (uint8_t)a, pc::RR_WEB);
  else if (cmd == "clr") session::remoteCommand(pc::CMD_CLEAR_ERR, 0, pc::RR_WEB);
  else if (cmd == "reboot") { sendText(200, "restarting"); g_restartAt = millis() + 600UL; return; }
  else { sendText(400, "unknown command"); return; }
  if (cmd == "relay" && arg == "1" && !store::cfg.remoteStart) { sendText(403, "Remote start is disabled in the settings (stop is always allowed)"); return; }
  sendText(200, "ok");
}

void handleWriteTag() {
  NEED_AUTH();
  const String text = srv.arg("tagText");
  if (text.length() == 0 || text.length() > MAX_TAG_TEXT) { sendText(400, "tagText missing or too long (max 38 characters)"); return; }
  nfc::queueWrite(text.c_str());
  sendText(200, "Hold the card on the reader within 30 s");
}

void handleWifi() {
  NEED_AUTH();
  const String ssid = srv.arg("ssid"), pass = srv.arg("pass");
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64 || (pass.length() > 0 && pass.length() < 8)) {
    sendText(400, "SSID 1..32 characters, password empty or 8..64 characters");
    return;
  }
  pc::copyStr(store::net.wifiSsid, ssid.c_str(), sizeof(store::net.wifiSsid));
  pc::copyStr(store::net.wifiPass, pass.c_str(), sizeof(store::net.wifiPass));
  if (!store::saveNet()) { sendText(500, "Failed to save"); return; }
  sendText(200, "Saved - connecting. The display shows the new address.");
  net::reconnect();
}

void handleScan() {
  NEED_AUTH();
  const int n = WiFi.scanNetworks();
  DynamicJsonDocument d(1536);
  JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < n && i < 15; i++) {
    JsonObject o = a.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendDoc(d);
}

void handleUpdate() {
  NEED_AUTH();
  const String act = srv.arg("action");
  if (fwupd::busy()) { sendText(409, "busy"); return; }
  if (act == "check") fwupd::request(false);
  else if (act == "install") fwupd::request(true);
  else { sendText(400, "action = check | install"); return; }
  sendText(200, "started");
}

void handleLog() {
  String out;
  for (int i = 0; i < plog::count(); i++) { out += plog::line(i); out += '\n'; }
  cors();
  srv.send(200, "text/plain", out);
}

void handleRoot() {
  if (net::apMode() && !net::wifiUp()) {           // provisioning: only the WiFi form
    cors();
    srv.send(200, "text/html", PAGE_SETUP);
    return;
  }
  srv.sendHeader("Content-Encoding", "gzip");
  srv.sendHeader("Cache-Control", "no-cache");
  srv.send_P(200, "text/html", (const char*)PAGE_INDEX_GZ, PAGE_INDEX_GZ_LEN);
}

// ── legacy v2 API (old dashboards / analyzer) ────────────────────────────────────────────────────────────────────
void handleLegacyStatus() {
  StaticJsonDocument<256> d;
  d["relayStatus"] = session::relayOn() ? "On" : "Off";
  d["buzzerStatus"] = "Off";
  d["buttonStatus"] = (buttons::down(1) || buttons::down(2)) ? "Pressed" : "Released";
  d["tagData"] = nfc::lastTag()[0] ? nfc::lastTag() : "No tag read";
  d["pendingWrite"] = nfc::writePending();
  sendDoc(d);
}

void handlePressCounter() {
  StaticJsonDocument<192> d;
  const uint32_t since = press::sinceLastStrokeMs();
  d["count"] = press::count();
  d["lastPulseDuration"] = press::lastDurationMs();
  d["lastPulseTime"] = (since != 0xFFFFFFFFUL && clk::valid()) ? clk::sec() - since / 1000UL : 0;
  d["inPulseTrain"] = press::inTrain();
  sendDoc(d);
}

}  // namespace

void routes() {
  srv.on("/", HTTP_GET, handleRoot);
  srv.on("/api/state", HTTP_GET, handleState);
  srv.on("/api/settings", HTTP_GET, handleGetSettings);
  srv.on("/api/settings", HTTP_POST, handlePostSettings);
  srv.on("/api/cmd", HTTP_POST, handleCmd);
  srv.on("/api/wifi", HTTP_POST, handleWifi);
  srv.on("/api/scan", HTTP_GET, handleScan);
  srv.on("/api/update", HTTP_POST, handleUpdate);
  srv.on("/api/log", HTTP_GET, handleLog);
  srv.on("/api/restart", HTTP_POST, []() { NEED_AUTH(); sendText(200, "restarting"); g_restartAt = millis() + 600UL; });
  srv.on("/write_tag", HTTP_POST, handleWriteTag);
  srv.on("/status", HTTP_GET, handleLegacyStatus);
  srv.on("/press_counter", HTTP_GET, handlePressCounter);
  srv.on("/relay", HTTP_POST, []() {
    NEED_AUTH();
    session::remoteCommand(pc::CMD_RELAY_TOGGLE, 0, pc::RR_WEB);
    sendText(200, session::relayOn() ? "Relay on" : "Relay off");
  });
  srv.on("/buzzer_error", HTTP_POST, []() { NEED_AUTH(); io::play(io::SND_ERROR); sendText(200, "ok"); });
  srv.on("/buzzer_warning", HTTP_POST, []() { NEED_AUTH(); io::play(io::SND_WARNING); sendText(200, "ok"); });
  srv.on("/buzzer_success", HTTP_POST, []() { NEED_AUTH(); io::play(io::SND_SUCCESS); sendText(200, "ok"); });
  srv.onNotFound([]() {
    if (srv.method() == HTTP_OPTIONS) { cors(); srv.send(204); return; }       // CORS pre-flight
    if (net::apMode() && !net::wifiUp()) { srv.sendHeader("Location", "http://192.168.4.1/"); srv.send(302); return; }   // captive portal
    sendText(404, "Not found");
  });
}

void begin() {
  routes();
  srv.begin();
  g_serving = true;
}

// The download/flash opens its own HTTP client.  Leaving the web server accepting clients while it
// does makes the board panic (two users of the same lwIP stack: an assert in the pbuf handling), so
// the server is stopped for the duration of an update and started again if the update fails.
// NB: do not call srv.stop()/srv.begin() here.  Stopping the WebServer from outside its own handler
// (the update is triggered by a POST that is still an open client) panicked the board every time the
// update was started from the web page; simply not servicing the server is enough to keep it out of
// the network stack while the download runs.
void pause() {
  if (!g_serving) return;
  g_serving = false;
  plog::log("web: page paused while the update runs");
}

void resume() {
  if (g_serving) return;
  g_serving = true;
  plog::log("web: page back up");
}

bool serving() { return g_serving; }

void loop() {
  if (!g_serving) return;                  // stopped while the update owns the network
  srv.handleClient();
  if (g_restartAt && (int32_t)(millis() - g_restartAt) >= 0) session::remoteCommand(pc::CMD_REBOOT, 0, pc::RR_WEB);
}

}  // namespace web
