#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

struct Config {
  String ssid;
  String password;
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPass;
  String mqttTopic;
  unsigned long displayTimeout;  // Timeout in seconds
};

class ConfigManager {
public:
  void init();
  bool load(Config& config);
  bool save(const Config& config);
  Config getConfig();

private:
  Config currentConfig;
  const char* configFile = "/config.json";
};

#endif