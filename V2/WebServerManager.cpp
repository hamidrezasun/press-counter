#include "WebServerManager.h"
#include <ArduinoJson.h>
#include <time.h>

static const size_t MAX_UPLOAD_BYTES = 200UL * 1024UL;     // index.html size limit (SPIFFS)

WebServerManager::WebServerManager(ESP8266WebServer& srv, ConfigManager& cfg, ControlManager& ctrl,
                                   NfcManager& nfc, PressCounter& pc)
  : _srv(srv), _config(cfg), _ctrl(ctrl), _nfc(nfc), _press(pc) {}

// The analyzer dashboard is served from another origin (port 8000) and calls this API: allow it.
static void cors(ESP8266WebServer& s) {
  s.sendHeader("Access-Control-Allow-Origin", "*");
  s.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  s.sendHeader("Access-Control-Allow-Headers", "*");
}

void WebServerManager::sendText(int code, const char* text) {
  cors(_srv);
  _srv.send(code, "text/plain", text);
}

void WebServerManager::sendJson(const JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  cors(_srv);
  _srv.send(200, "application/json", out);
}

void WebServerManager::routes() {
  _srv.on("/", HTTP_GET, [this]() { handleRoot(); });
  _srv.on("/upload", HTTP_POST, [this]() { sendText(200, "Upload complete"); }, [this]() { handleUpload(); });
  _srv.on("/save", HTTP_GET, [this]() { handleGetSettings(); });
  _srv.on("/save", HTTP_POST, [this]() { handleSaveSettings(); });
  _srv.on("/status", HTTP_GET, [this]() { handleStatus(); });
  _srv.on("/press_counter", HTTP_GET, [this]() { handlePressCounter(); });
  _srv.on("/write_tag", HTTP_POST, [this]() { handleWriteTag(); });
  _srv.on("/status/file", HTTP_GET, [this]() {
    StaticJsonDocument<48> d;
    d["fileExists"] = SPIFFS.exists("/index.html");
    sendJson(d);
  });
  _srv.on("/relay", HTTP_POST, [this]() {
    _ctrl.toggleRelay("web");
    sendText(200, _ctrl.relayOn() ? "Relay on" : "Relay off");
  });
  _srv.on("/buzzer_error",   HTTP_POST, [this]() { _ctrl.playErrorSound();   sendText(200, "ok"); });
  _srv.on("/buzzer_warning", HTTP_POST, [this]() { _ctrl.playWarningSound(); sendText(200, "ok"); });
  _srv.on("/buzzer_success", HTTP_POST, [this]() { _ctrl.playSuccessSound(); sendText(200, "ok"); });
  _srv.onNotFound([this]() {
    if (_srv.method() == HTTP_OPTIONS) { cors(_srv); _srv.send(204); }      // CORS pre-flight
    else sendText(404, "Not found");
  });
}

void WebServerManager::handleRoot() {
  if (SPIFFS.exists("/index.html")) {
    File f = SPIFFS.open("/index.html", "r");
    if (f) { _srv.streamFile(f, "text/html"); f.close(); return; }
  }
  // no interface uploaded yet: tiny upload form
  _srv.send(200, "text/html",
    F("<!DOCTYPE html><meta charset=utf-8><title>Setup</title><body style='font-family:sans-serif;padding:20px'>"
      "<h2>No interface found</h2><p>Upload <code>index.html</code>:</p>"
      "<form action='/upload' method='POST' enctype='multipart/form-data'>"
      "<input type='file' name='htmlFile' accept='.html'><button>Upload</button></form></body>"));
}

void WebServerManager::handleUpload() {
  HTTPUpload& up = _srv.upload();
  if (up.status == UPLOAD_FILE_START) {
    _uploadBytes = 0;
    _upload = SPIFFS.open("/index.html", "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    _uploadBytes += up.currentSize;
    if (_upload && _uploadBytes > MAX_UPLOAD_BYTES) { _upload.close(); SPIFFS.remove("/index.html"); }
    if (_upload) _upload.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (_upload) _upload.close();
  }
}

void WebServerManager::handleGetSettings() {
  const Config& c = _config.get();
  StaticJsonDocument<512> d;
  d["device"] = c.device;
  d["influxHost"] = c.influxHost;
  d["influxPort"] = c.influxPort;
  d["influxBucket"] = c.influxBucket;
  d["influxOrg"] = c.influxOrg;
  d["influxTokenSet"] = c.influxToken[0] != 0;      // the token itself never leaves the device
  d["minPulseDuration"] = c.minPulseDuration;
  sendJson(d);
}

void WebServerManager::handleSaveSettings() {
  Config c = _config.get();
  bool changed = false;

  auto text = [&](const char* arg, char* dst, size_t n) {
    if (_srv.hasArg(arg) && _srv.arg(arg).length() > 0) { strlcpy(dst, _srv.arg(arg).c_str(), n); changed = true; }
  };
  text("device", c.device, sizeof(c.device));
  text("influxHost", c.influxHost, sizeof(c.influxHost));
  text("influxBucket", c.influxBucket, sizeof(c.influxBucket));
  text("influxOrg", c.influxOrg, sizeof(c.influxOrg));
  text("influxToken", c.influxToken, sizeof(c.influxToken));
  text("otaPass", c.otaPassword, sizeof(c.otaPassword));

  if (_srv.hasArg("influxPort") && _srv.arg("influxPort").length()) {
    const int port = _srv.arg("influxPort").toInt();
    if (port < 1 || port > 65535) { sendText(400, "Invalid port"); return; }
    c.influxPort = port;
    changed = true;
  }
  if (_srv.hasArg("minPulseDuration") && _srv.arg("minPulseDuration").length()) {
    const int ms = _srv.arg("minPulseDuration").toInt();
    if (ms < 1 || ms > 5000) { sendText(400, "minPulseDuration must be 1..5000 ms"); return; }
    c.minPulseDuration = ms;
    changed = true;
  }
  if (!changed) { sendText(400, "No valid settings provided"); return; }

  _config.set(c);
  if (!_config.save()) { sendText(500, "Failed to save settings"); return; }
  _press.updateMinPulseDuration();
  if (_srv.hasArg("otaPass") && _srv.arg("otaPass").length()) ArduinoOTA.setPassword(_config.get().otaPassword);
  sendText(200, "Settings saved");
}

void WebServerManager::handleStatus() {
  const DeviceStatus s = _ctrl.getStatus();
  const char* tag = _nfc.getLastTagData();
  StaticJsonDocument<384> d;
  d["relayStatus"] = s.relayOn ? "On" : "Off";
  d["buzzerStatus"] = s.buzzerOn ? "On" : "Off";
  d["buttonStatus"] = s.buttonPressed ? "Pressed" : "Released";
  d["tagData"] = tag[0] ? tag : "No tag read";
  d["pendingWrite"] = _nfc.isPendingWrite();
  sendJson(d);
}

void WebServerManager::handlePressCounter() {
  // Dashboards expect a Unix timestamp (s); derive it from NTP time when the clock is valid.
  unsigned long lastUnix = 0;
  const unsigned long lastMs = _press.getLastPulseTime();
  const time_t now = time(nullptr);
  if (lastMs && now > 1700000000L) lastUnix = (unsigned long)now - (millis() - lastMs) / 1000UL;

  StaticJsonDocument<192> d;
  d["count"] = _press.getCount();
  d["lastPulseDuration"] = _press.getLastPulseDuration();
  d["lastPulseTime"] = lastUnix;
  d["inPulseTrain"] = _press.getInPulseTrain();
  sendJson(d);
}

void WebServerManager::handleWriteTag() {
  const String text = _srv.arg("tagText");
  if (text.length() == 0 || text.length() >= MAX_TAG_DATA_LENGTH) {
    _ctrl.playErrorSound();
    sendText(400, "tagText missing or too long");
    return;
  }
  _nfc.setPendingWrite(text.c_str());
  sendText(200, "Write queued - present the tag within 30 s");
}

void WebServerManager::begin() {
  routes();
  _srv.begin();

  ArduinoOTA.setHostname(_config.get().device);
  ArduinoOTA.setPassword(_config.get().otaPassword);
  ArduinoOTA.onStart([]() { Serial.println("[OTA] start"); });
  ArduinoOTA.onEnd([]() { Serial.println("[OTA] done"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[OTA] error %u\n", (unsigned)e); });
  ArduinoOTA.begin();
  Serial.println("[Web] HTTP server on :80, OTA enabled");
}

void WebServerManager::handleClient() {
  _srv.handleClient();
  ArduinoOTA.handle();
}
