#include "Config.h"
#include <LittleFS.h>

bool Config::load() {
  if (!LittleFS.exists("/config.json")) {
    Serial.println("Config file does not exist, creating with default values");
    if (!save()) {
      Serial.println("Failed to create config file with default values");
      return false;
    }
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println("Failed to parse config file");
    configFile.close();
    return false;
  }

  ntp_server = doc["ntp_server"].as<String>();
  debounce_delay = doc["debounce_delay"];
  device_id = doc["device_id"].as<String>();
  wifi_ssid = doc["wifi_ssid"].as<String>();
  wifi_password = doc["wifi_password"].as<String>();
  mqtt_server = doc["mqtt_server"].as<String>();
  mqtt_port = doc["mqtt_port"];
  mqtt_counting_topic = doc["mqtt_counting_topic"].as<String>();
  mqtt_nfc_topic = doc["mqtt_nfc_topic"].as<String>();
  mqtt_device_info_topic = doc["mqtt_device_info_topic"].as<String>();
  mqtt_turnon_topic = doc["mqtt_turnon_topic"].as<String>();
  mqtt_button_topic = doc["mqtt_button_topic"].as<String>();
  mqtt_username = doc["mqtt_username"].as<String>();
  mqtt_password = doc["mqtt_password"].as<String>();
  counting = doc["counting"] | false;

  Serial.print("Loaded ntp_server: ");
  Serial.println(ntp_server);
  Serial.print("Loaded device_id: ");
  Serial.println(device_id);
  Serial.print("Loaded wifi_ssid: ");
  Serial.println(wifi_ssid);
  Serial.println("Config loaded");
  
  configFile.close();
  return true;
}

bool Config::save() {
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["ntp_server"] = ntp_server;
  doc["debounce_delay"] = debounce_delay;
  doc["device_id"] = device_id;
  doc["wifi_ssid"] = wifi_ssid;
  doc["wifi_password"] = wifi_password;
  doc["mqtt_server"] = mqtt_server;
  doc["mqtt_port"] = mqtt_port;
  doc["mqtt_counting_topic"] = mqtt_counting_topic;
  doc["mqtt_nfc_topic"] = mqtt_nfc_topic;
  doc["mqtt_device_info_topic"] = mqtt_device_info_topic;
  doc["mqtt_turnon_topic"] = mqtt_turnon_topic;
  doc["mqtt_button_topic"] = mqtt_button_topic;
  doc["mqtt_username"] = mqtt_username;
  doc["mqtt_password"] = mqtt_password;
  doc["counting"] = counting;

  serializeJson(doc, configFile);
  
  Serial.println("Config saved");
  
  configFile.close();
  return true;
}

void Config::setCounting(bool state) {
  counting = state;
  if (!save()) {
    Serial.println("Failed to save counting state");
  }
}