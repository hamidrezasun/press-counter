#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "ConfigManager.h"
#include "MQTTHandler.h"
#include "DisplayHandler.h"
#include "HardwareHandler.h"

class WebServer {
public:
  void init(ConfigManager* config, MQTTHandler* mqtt, DisplayHandler* display, HardwareHandler* hardware);
  void handle();

private:
  AsyncWebServer server{80};
  ConfigManager* configMgr;
  MQTTHandler* mqttHdlr;
  DisplayHandler* displayHdlr;
  HardwareHandler* hardwareHdlr;

  void handleApiGet(AsyncWebServerRequest *request);
  void handleApiPost(AsyncWebServerRequest *request);
  void handleMessage(AsyncWebServerRequest *request);
  void handleVibrate(AsyncWebServerRequest *request);
  void handleBeep(AsyncWebServerRequest *request);
  void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
};

#endif