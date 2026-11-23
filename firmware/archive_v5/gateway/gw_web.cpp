#include "gw_web.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <time.h>
#include "gw_config.h"
#include "gw_link.h"
#include "gw_page.h"
#include "gw_settings.h"
#include "gw_state.h"
#include "gw_telemetry.h"
#include "gw_update.h"

namespace web {
namespace {

ESP8266WebServer srv(80);
bool g_wifiChanged = false;
uint32_t g_restartAt = 0;
uint32_t g_pendingWriteUntil = 0;
File g_upload;
uint32_t g_uploadBytes = 0;

const size_t MAX_UPLOAD = 200UL * 1024UL;
const char* STM_KEYS[] = {"name", "minpulse", "idle", "stuck", "tempoff", "overtemp", "manual", "remote", "sound", "bright", "display", "out1", "out2", "out3", "out4", "v12cal", "accal"};

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

// "admin" + adminPass guards everything that changes something.  Without an admin password the device is open (LAN only!).
bool authed() {
  const char* p = settings::cfg.adminPass;
  if (!p[0]) return true;
  if (srv.authenticate("admin", p)) return true;
  srv.requestAuthentication(BASIC_AUTH, "press counter");
  return false;
}
#define NEED_AUTH() do { if (!authed()) return; } while (0)

const char* devName() { return gws::deviceName(); }

// ── legacy v2 API ──────────────────────────────────────────────────────────────────────────────────────────────
void handleStatus() {
  const StmStatus& s = gwlink::status();
  StaticJsonDocument<320> d;
  d["relayStatus"] = (s.valid && s.relay) ? "On" : "Off";
  d["buzzerStatus"] = "Off";
  d["buttonStatus"] = "Released";
  d["tagData"] = gws::lastTag[0] ? gws::lastTag : "No tag read";
  d["pendingWrite"] = (int32_t)(g_pendingWriteUntil - millis()) > 0;
  sendDoc(d);
}

void handlePressCounter() {
  const StmStatus& s = gwlink::status();
  unsigned long lastUnix = 0;
  const time_t now = time(nullptr);
  if (s.valid && s.lastAgoS >= 0 && now > 1700000000L) lastUnix = (unsigned long)now - (unsigned long)s.lastAgoS;
  StaticJsonDocument<192> d;
  d["count"] = s.valid ? s.count : 0;
  d["lastPulseDuration"] = s.valid ? s.lastDur : 0;
  d["lastPulseTime"] = lastUnix;
  d["inPulseTrain"] = s.valid && s.inPress;
  sendDoc(d);
}

void handleGetSave() {
  const GwSettings& c = settings::cfg;
  char v[16];
  StaticJsonDocument<512> d;
  d["device"] = devName();
  d["minPulseDuration"] = gwlink::getSetting("minpulse", v, sizeof(v)) ? pc::toInt(v) : 0;
  sendDoc(d);
}

void applyText(const char* arg, char* dst, size_t n, bool& changed) {
  if (srv.hasArg(arg) && srv.arg(arg).length() > 0) { pc::copyStr(dst, srv.arg(arg).c_str(), n); changed = true; }
}

void handlePostSave() {
  NEED_AUTH();
  GwSettings& c = settings::cfg;
  bool changed = false;
  applyText("otaPass", c.otaPass, sizeof(c.otaPass), changed);
  if (srv.hasArg("device") && srv.arg("device").length()) {
    char clean[24];
    pc::sanitize(clean, sizeof(clean), srv.arg("device").c_str());
    pc::copyStr(c.device, clean, sizeof(c.device));
    gwlink::setSetting("name", clean);
    changed = true;
  }
  if (srv.hasArg("minPulseDuration") && srv.arg("minPulseDuration").length()) {
    const int ms = srv.arg("minPulseDuration").toInt();
    if (ms < 1 || ms > 5000) { sendText(400, "minPulseDuration must be 1..5000 ms"); return; }
    char b[8];
    snprintf(b, sizeof(b), "%d", ms);
    gwlink::setSetting("minpulse", b);
    changed = true;
  }
  if (!changed) { sendText(400, "No valid settings provided"); return; }
  if (!settings::save()) { sendText(500, "Failed to save settings"); return; }
  tele::reconfigure();
  sendText(200, "Settings saved");
}

void handleWriteTag() {
  NEED_AUTH();
  const String text = srv.arg("tagText");
  if (text.length() == 0 || text.length() > 40) { sendText(400, "tagText missing or too long"); return; }
  char clean[48], b[64];
  pc::sanitize(clean, sizeof(clean), text.c_str());
  snprintf(b, sizeof(b), "W,%s", clean);
  gwlink::send(b);
  g_pendingWriteUntil = millis() + 30000UL;
  sendText(200, "Write queued - present the tag within 30 s");
}

void kcmd(const char* cmd, const char* arg = "") {
  char b[48];
  if (arg[0]) snprintf(b, sizeof(b), "K,%s,%s", cmd, arg);
  else snprintf(b, sizeof(b), "K,%s", cmd);
  gwlink::send(b);
}

void handleUploadPage() {
  HTTPUpload& up = srv.upload();
  if (up.status == UPLOAD_FILE_START) {
    g_uploadBytes = 0;
    g_upload = LittleFS.open("/index.html", "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    g_uploadBytes += up.currentSize;
    if (g_upload && g_uploadBytes > MAX_UPLOAD) { g_upload.close(); LittleFS.remove("/index.html"); }
    if (g_upload) g_upload.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (g_upload) g_upload.close();
  }
}

// ── new JSON API ───────────────────────────────────────────────────────────────────────────────────────────────
void handleState() {
  const StmStatus& s = gwlink::status();
  const StmIdentity& id = gwlink::identity();
  const fwupd::Info& u = fwupd::info();
  DynamicJsonDocument d(3600);
  d["device"] = devName();
  d["version"] = GW_VERSION;
  d["uptime"] = millis() / 1000UL;
  d["heap"] = ESP.getFreeHeap();
  d["time"] = (uint32_t)time(nullptr);
  d["tag"] = gws::lastTag;
  JsonObject net = d.createNestedObject("net");
  net["wifi"] = WiFi.status() == WL_CONNECTED;
  net["ap"] = gws::apMode;
  net["ssid"] = WiFi.SSID();
  net["ip"] = gws::apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  net["rssi"] = WiFi.RSSI();
  net["mac"] = WiFi.macAddress();
  net["ntp"] = gws::ntpOk;
  net["server"] = tele::serverOk();
  net["mqtt"] = tele::mqttOk();
  net["mqttConfigured"] = tele::mqttConfigured();
  JsonObject m = d.createNestedObject("stm");
  m["alive"] = gwlink::alive();
  m["events"] = gwlink::eventsReceived();
  m["badFrames"] = gwlink::badFrames();
  if (id.valid) { m["name"] = id.name; m["ver"] = id.ver; m["flags"] = id.flags; }
  if (s.valid) {
    m["relay"] = s.relay;
    m["count"] = s.count;
    m["lastDurMs"] = s.lastDur;
    m["inPress"] = s.inPress;
    if (s.tempC10 != 9999) m["tempC"] = s.tempC10 / 10.0f; else m["tempC"] = nullptr;
    m["err"] = s.topErr;
    m["errText"] = pc::errorText(s.topErr);
    m["errMask"] = s.errMask;
    m["operator"] = s.op;
    m["die"] = s.die;
    m["sessionSec"] = s.sessSec;
    m["mcuUptime"] = s.uptime;
    m["queued"] = s.queued;
    m["lastAgoS"] = s.lastAgoS;
    m["lifetime"] = s.lifetime;
    m["calls"] = s.calls;
    m["vdda"] = s.vdda;
    m["mcuTempC"] = s.mcuT10 / 10.0f;
    m["outs"] = s.outs;
    m["v12"] = s.v12mV / 1000.0f;
    m["v230"] = s.vac10 / 10.0f;
  }
  JsonObject up = d.createNestedObject("update");
  up["phase"] = u.phase;
  up["pct"] = u.pct;
  up["msg"] = u.msg;
  up["espLatest"] = u.espLatest;
  up["stmLatest"] = u.stmLatest;
  up["espNew"] = u.espNew;
  up["stmNew"] = u.stmNew;
  up["configured"] = settings::cfg.updateUrl[0] != 0;
  up["busy"] = fwupd::busy();
  sendDoc(d);
}

void handleGetSettings() {
  NEED_AUTH();
  const GwSettings& c = settings::cfg;
  DynamicJsonDocument d(2400);
  JsonObject g = d.createNestedObject("gateway");
  g["device"] = c.device;
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
  JsonObject m = d.createNestedObject("stm");
  char v[32];
  for (const char* k : STM_KEYS) if (gwlink::getSetting(k, v, sizeof(v))) m[k] = v;
  sendDoc(d);
}

// JSON body: {"gateway":{...},"stm":{"minpulse":"20",...}}.  Secrets are only replaced by a non-empty value.
void handlePostSettings() {
  NEED_AUTH();
  DynamicJsonDocument d(2400);
  if (deserializeJson(d, srv.arg("plain"))) { sendText(400, "bad JSON"); return; }
  GwSettings& c = settings::cfg;
  JsonObject g = d["gateway"];
  auto s = [&](const char* k, char* dst, size_t n, bool secret = false) {
    if (!g.containsKey(k)) return;
    const char* v = g[k] | "";
    if (secret && !v[0]) return;
    pc::copyStr(dst, v, n);
  };
  s("device", c.device, sizeof(c.device));
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
  if (!settings::save()) { sendText(500, "Failed to save settings"); return; }

  JsonObject m = d["stm"];
  int sent = 0;
  for (JsonPair kv : m) {
    char val[40];
    if (kv.value().is<const char*>()) pc::copyStr(val, kv.value().as<const char*>(), sizeof(val));
    else snprintf(val, sizeof(val), "%ld", (long)kv.value().as<long>());
    char clean[40];
    pc::sanitize(clean, sizeof(clean), val);
    if (gwlink::setSetting(kv.key().c_str(), clean)) sent++;
  }
  if (sent) gwlink::requestSettings();
  tele::reconfigure();
  StaticJsonDocument<64> r;
  r["ok"] = true;
  r["stmSent"] = sent;
  sendDoc(r);
}

void handleCmd() {
  NEED_AUTH();
  const String cmd = srv.arg("cmd"), arg = srv.arg("arg");
  static const char* const OK[] = {"relay", "reset", "buzz", "call", "ack", "clr", "reboot", "out"};
  bool known = false;
  for (const char* k : OK) if (cmd == k) known = true;
  if (!known) { sendText(400, "unknown command"); return; }
  char a[16];
  pc::sanitize(a, sizeof(a), arg.c_str());
  kcmd(cmd.c_str(), a);
  sendText(200, "sent");
}

void handleWifi() {
  NEED_AUTH();
  const String ssid = srv.arg("ssid"), pass = srv.arg("pass");
  if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64 || (pass.length() > 0 && pass.length() < 8)) {
    sendText(400, "SSID 1..32 characters, password empty or 8..64 characters");
    return;
  }
  pc::copyStr(settings::cfg.wifiSsid, ssid.c_str(), sizeof(settings::cfg.wifiSsid));
  pc::copyStr(settings::cfg.wifiPass, pass.c_str(), sizeof(settings::cfg.wifiPass));
  if (!settings::save()) { sendText(500, "Failed to save"); return; }
  sendText(200, "Saved - connecting");
  g_wifiChanged = true;
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
    o["open"] = WiFi.encryptionType(i) == ENC_TYPE_NONE;
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
  else if (act == "retry") fwupd::requestStmRetry();
  else { sendText(400, "action = check | install | retry"); return; }
  sendText(200, "started");
}

void handleLog() {
  String out;
  for (int i = 0; i < gws::logCount(); i++) { out += gws::logLine(i); out += '\n'; }
  cors();
  srv.send(200, "text/plain", out);
}

void handleRoot() {
  if (gws::apMode) {                                // provisioning: only the WiFi form
    cors();
    srv.send_P(200, "text/html", PAGE_SETUP);
    return;
  }
  if (LittleFS.exists("/index.html")) {              // a page uploaded by hand overrides the built-in one
    File f = LittleFS.open("/index.html", "r");
    if (f) { srv.streamFile(f, "text/html"); f.close(); return; }
  }
  srv.sendHeader("Content-Encoding", "gzip");
  srv.sendHeader("Cache-Control", "no-cache");
  srv.send_P(200, "text/html", (PGM_P)PAGE_INDEX_GZ, PAGE_INDEX_GZ_LEN);
}

}  // namespace

bool takeWifiChange() { const bool r = g_wifiChanged; g_wifiChanged = false; return r; }

void begin() {
  srv.on("/", HTTP_GET, handleRoot);
  srv.on("/upload", HTTP_POST, []() { sendText(200, "Upload complete"); }, handleUploadPage);
  srv.on("/save", HTTP_GET, handleGetSave);
  srv.on("/save", HTTP_POST, handlePostSave);
  srv.on("/status", HTTP_GET, handleStatus);
  srv.on("/press_counter", HTTP_GET, handlePressCounter);
  srv.on("/write_tag", HTTP_POST, handleWriteTag);
  srv.on("/status/file", HTTP_GET, []() {
    StaticJsonDocument<48> d;
    d["fileExists"] = true;                          // the built-in page is always there
    sendDoc(d);
  });
  srv.on("/relay", HTTP_POST, []() {
    NEED_AUTH();
    const StmStatus& s = gwlink::status();
    kcmd("relay");
    sendText(200, (s.valid && s.relay) ? "Relay off" : "Relay on");   // the STM32 decides (remote start may be disabled)
  });
  srv.on("/buzzer_error", HTTP_POST, []() { NEED_AUTH(); kcmd("buzz", "error"); sendText(200, "ok"); });
  srv.on("/buzzer_warning", HTTP_POST, []() { NEED_AUTH(); kcmd("buzz", "warning"); sendText(200, "ok"); });
  srv.on("/buzzer_success", HTTP_POST, []() { NEED_AUTH(); kcmd("buzz", "success"); sendText(200, "ok"); });

  srv.on("/api/state", HTTP_GET, handleState);
  srv.on("/api/settings", HTTP_GET, handleGetSettings);
  srv.on("/api/settings", HTTP_POST, handlePostSettings);
  srv.on("/api/cmd", HTTP_POST, handleCmd);
  srv.on("/api/wifi", HTTP_POST, handleWifi);
  srv.on("/api/scan", HTTP_GET, handleScan);
  srv.on("/api/update", HTTP_POST, handleUpdate);
  srv.on("/api/log", HTTP_GET, handleLog);
  srv.on("/api/restart", HTTP_POST, []() { NEED_AUTH(); sendText(200, "restarting"); g_restartAt = millis() + 600UL; });

  srv.onNotFound([]() {
    if (srv.method() == HTTP_OPTIONS) { cors(); srv.send(204); return; }       // CORS pre-flight
    if (gws::apMode) { srv.sendHeader("Location", "http://192.168.4.1/"); srv.send(302); return; }   // captive portal
    sendText(404, "Not found");
  });
  srv.begin();
}

void loop() {
  srv.handleClient();
  if (g_restartAt && (int32_t)(millis() - g_restartAt) >= 0) ESP.restart();
}

}  // namespace web
