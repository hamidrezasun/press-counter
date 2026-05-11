#ifndef MQTTHANDLER_H
#define MQTTHANDLER_H

#include <PubSubClient.h>
#include <WiFi.h>
#include "ConfigManager.h"
#include "DisplayHandler.h"
#include "HardwareHandler.h"

class MQTTHandler {
public:
  void init(ConfigManager* config, DisplayHandler* display, HardwareHandler* hardware);
  void loop();

private:
  WiFiClient espClient;
  PubSubClient client{espClient};
  ConfigManager* configMgr;
  DisplayHandler* displayHdlr;
  HardwareHandler* hardwareHdlr;

  void connect();
  static void callback(char* topic, byte* payload, unsigned int length);
};

#endif