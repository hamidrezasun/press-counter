#include "ConfigManager.h"

ConfigManager::ConfigManager(const char* filename) : _filename(filename) {}

bool ConfigManager::load() {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return false;
  }
  if (!SPIFFS.exists(_filename)) {
    Serial.println("No config file, using defaults");
    setDefaults();
    return false;
  }
  File file = SPIFFS.open(_filename, "r");
  if (!file) {
    Serial.println("Failed to open config file");
    setDefaults();
    return false;
  }
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("Failed to parse config, using defaults");
    setDefaults();
    return false;
  }
  config.device = doc["device"] | "";
  config.influxHost = doc["influxHost"] | "";
  config.influxPort = doc["influxPort"] | 8086;
  config.influxBucket = doc["influxBucket"] | "";
  config.influxOrg = doc["influxOrg"] | "";
  config.influxToken = doc["influxToken"] | "";
  config.ntp = doc["ntp"] | "";
  config.debounceDelay = doc["debounceDelay"] | 0;
  config.firmwareUrl = doc["firmwareUrl"] | "http://server/file.bin";
  return true;
}

bool ConfigManager::save() {
  StaticJsonDocument<512> doc;
  doc["device"] = config.device;
  doc["influxHost"] = config.influxHost;
  doc["influxPort"] = config.influxPort;
  doc["influxBucket"] = config.influxBucket;
  doc["influxOrg"] = config.influxOrg;
  doc["influxToken"] = config.influxToken;
  doc["ntp"] = config.ntp;
  doc["debounceDelay"] = config.debounceDelay;
  doc["firmwareUrl"] = config.firmwareUrl;
  File file = SPIFFS.open(_filename, "w");
  if (!file) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  serializeJson(doc, file);
  file.close();
  Serial.println("Config saved");
  return true;
}

void ConfigManager::setDefaults() {
  config.device = "device";
  config.influxHost = "";
  config.influxPort = 8086;
  config.influxBucket = "";
  config.influxOrg = "";
  config.influxToken = "";
  config.ntp = "";
  config.debounceDelay = 0;
  config.firmwareUrl = "http://server/file.bin";
}

Config& ConfigManager::get() { return config; }