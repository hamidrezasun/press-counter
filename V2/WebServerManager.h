// WebServerManager - REST API used by V2/index.html (served from SPIFFS) and the analyzer dashboard, plus OTA.
#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "ControlManager.h"
#include "NfcManager.h"
#include "PressCounter.h"

class WebServerManager {
public:
  WebServerManager(ESP8266WebServer& srv, ConfigManager& cfg, ControlManager& ctrl, NfcManager& nfc, PressCounter& pc);
  void begin();
  void handleClient();                 // HTTP + OTA

private:
  ESP8266WebServer& _srv;
  ConfigManager&    _config;
  ControlManager&   _ctrl;
  NfcManager&       _nfc;
  PressCounter&     _press;
  File   _upload;
  size_t _uploadBytes = 0;

  void sendJson(const JsonDocument& doc);
  void sendText(int code, const char* text);
  void routes();
  void handleRoot();
  void handleUpload();
  void handleGetSettings();
  void handleSaveSettings();
  void handleStatus();
  void handlePressCounter();
  void handleWriteTag();
};

#endif
