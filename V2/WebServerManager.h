#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <ESPAsyncWebServer.h>
#include <ESPUI.h>
#include "ConfigManager.h"
#include "ControlManager.h"
#include "NfcManager.h"

struct CallbackData {
  ConfigManager* configManager;
  int id;
};

class WebServerManager {
public:
  WebServerManager(AsyncWebServer& srv, ConfigManager& cfg, ControlManager& ctrl, NfcManager& nfc);
  void begin();
  void updateStatus();

private:
  AsyncWebServer& server;
  ConfigManager& configManager;
  ControlManager& controlManager;
  NfcManager& nfcManager;
  uint16_t relayLabelId;
  uint16_t buzzerLabelId;
  uint16_t buttonLabelId;
  uint16_t tagDataLabelId;
  uint16_t deviceNameId;
  uint16_t influxHostId;
  uint16_t influxPortId;
  uint16_t influxBucketId;
  uint16_t influxOrgId;
  uint16_t influxTokenId;
  uint16_t ntpId;
  uint16_t debounceDelayId;
  uint16_t firmwareUrlId;
  uint16_t firmwareUploadStatusId;
  uint16_t firmwareUploadButtonId;
  void setupUI();
  void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
  static void onSaveSettings(Control* control, int id, void* userData);
  static void onRelayToggle(Control* control, int id, void* userData);
  static void onBuzzerError(Control* control, int id, void* userData);
  static void onBuzzerWarning(Control* control, int id, void* userData);
  static void onBuzzerSuccess(Control* control, int id, void* userData);
  static void onWriteTag(Control* control, int id, void* userData);
  static void onFirmwareUploadButton(Control* control, int id, void* userData);
};

#endif