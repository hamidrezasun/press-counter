#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include "ConfigManager.h"
#include "ControlManager.h"
#include "NfcManager.h"
#include "PressCounter.h"

struct CallbackData {
  ConfigManager* configManager;
  int id;
};

class WebServerManager {
public:
  WebServerManager(ESP8266WebServer& srv, ConfigManager& cfg, ControlManager& ctrl, NfcManager& nfc, PressCounter& pc);
  void begin();
  void updateStatus();
  void handleClient();

private:
  ESP8266WebServer& server;
  ConfigManager& configManager;
  ControlManager& controlManager;
  NfcManager& nfcManager;
  PressCounter& pressCounter;
  String lastTagData;
  File uploadFile;
  void setupRoutes();
  void handleRoot();
  void handleUploadHtml();
  void handleSaveSettings();
  void handleRelayToggle();
  void handleBuzzerError();
  void handleBuzzerWarning();
  void handleBuzzerSuccess();
  void handleWriteTag();
  void handleStatus();
  void handleFileStatus();
  void handleGetSettings();
  void handlePressCounterStatus();
};

#endif