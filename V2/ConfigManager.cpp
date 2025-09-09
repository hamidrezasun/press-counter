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
  
  strlcpy(config.device, doc["device"] | "device", 10);
  strlcpy(config.influxHost, doc["influxHost"] | "", 20);
  config.influxPort = doc["influxPort"] | 8086;
  strlcpy(config.influxBucket, doc["influxBucket"] | "", 20);
  strlcpy(config.influxOrg, doc["influxOrg"] | "", 20);
  strlcpy(config.influxToken, doc["influxToken"] | "", 90);
  config.minPulseDuration = doc["minPulseDuration"] | 10;
  
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
  doc["minPulseDuration"] = config.minPulseDuration;
  
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
  strlcpy(config.device, "device", 10);
  strlcpy(config.influxHost, "", 20);
  config.influxPort = 8086;
  strlcpy(config.influxBucket, "", 20);
  strlcpy(config.influxOrg, "", 20);
  strlcpy(config.influxToken, "", 90);
  config.minPulseDuration = 10;
}

Config& ConfigManager::get() { return config; }

void ConfigManager::set(const Config& newConfig) {
  config = newConfig;
}