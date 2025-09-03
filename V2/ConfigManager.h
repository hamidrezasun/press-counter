#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>


struct Config {
  char device[10];
  char influxHost[20];
  int influxPort;
  char influxBucket[20];
  char influxOrg[20];
  char influxToken[90];
  int debounceDelay;
};

class ConfigManager {
public:
  ConfigManager(const char* filename = "/config.json");
  bool load();
  bool save();
  void setDefaults();
  Config& get();
  void set(const Config& newConfig);
private:
  const char* _filename;
  Config config;
};

#endif