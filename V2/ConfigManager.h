#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

struct Config {
  String device;
  String influxHost;
  int influxPort;
  String influxBucket;
  String influxOrg;
  String influxToken;
  String ntp;
  int debounceDelay;
  String firmwareUrl;
};

class ConfigManager {
public:
  ConfigManager(const char* filename = "/config.json");
  bool load();
  bool save();
  void setDefaults();
  Config& get();

private:
  const char* _filename;
  Config config;
};

#endif