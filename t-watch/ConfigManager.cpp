#include "ConfigManager.h"
#include "config.h"

void ConfigManager::init() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  Serial.println("LittleFS Mounted");
  load(currentConfig);
}

bool ConfigManager::load(Config& config) {
  if (!LittleFS.exists(configFile)) {
    Serial.println("No config file, using defaults");
    config.ssid = "";
    config.password = "";
    config.mqttServer = "broker.hivemq.com";
    config.mqttPort = 1883;
    config.mqttUser = "";
    config.mqttPass = "";
    config.mqttTopic = "watch/message";
    config.displayTimeout = 30;  // Default 30 seconds
    save(config);
    return false;
  }

  File file = LittleFS.open(configFile, "r");
  if (!file) return false;

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, file);
  file.close();

  config.ssid = doc["ssid"] | "";
  config.password = doc["password"] | "";
  config.mqttServer = doc["mqttServer"] | "broker.hivemq.com";
  config.mqttPort = doc["mqttPort"] | 1883;
  config.mqttUser = doc["mqttUser"] | "";
  config.mqttPass = doc["mqttPass"] | "";
  config.mqttTopic = doc["mqttTopic"] | "watch/message";
  config.displayTimeout = doc["displayTimeout"] | 30;

  Serial.println("Config loaded");
  return true;
}

bool ConfigManager::save(const Config& config) {
  File file = LittleFS.open(configFile, "w");
  if (!file) return false;

  DynamicJsonDocument doc(1024);
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["mqttServer"] = config.mqttServer;
  doc["mqttPort"] = config.mqttPort;
  doc["mqttUser"] = config.mqttUser;
  doc["mqttPass"] = config.mqttPass;
  doc["mqttTopic"] = config.mqttTopic;
  doc["displayTimeout"] = config.displayTimeout;

  serializeJson(doc, file);
  file.close();

  Serial.println("Config saved");
  return true;
}

Config ConfigManager::getConfig() {
  return currentConfig;
}