#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESP8266WebServer.h>
#include <FS.h>
#include "ConfigManager.h"
#include "ControlManager.h"
#include "NfcManager.h"

struct CallbackData {
  ConfigManager* configManager;
  int id;
};

class WebServerManager {
public:
  WebServerManager(ESP8266WebServer& srv, ConfigManager& cfg, ControlManager& ctrl, NfcManager& nfc);
  void begin();
  void updateStatus();
  void handleClient();

private:
  ESP8266WebServer& server;
  ConfigManager& configManager;
  ControlManager& controlManager;
  NfcManager& nfcManager;
  String lastTagData;
  File uploadFile; // Member variable to persist file handle
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
  void handleUpdate();
  void handleUploadFirmware();
  void handleFileStatus();
  void handleGetSettings();
};

#endif