#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <FS.h>
#include <ArduinoJson.h>
#include "define.h"

#define CONFIG_FILE "/config.json"

void initFilesystem() {
  SPIFFS.begin();
}

bool saveConfig() {
  File file = SPIFFS.open(CONFIG_FILE, "w");
  if (!file) {
    return false;
  }

  DynamicJsonDocument doc(512);
  doc["WIFI_SSID"] = WIFI_SSID;
  doc["WIFI_PASSWORD"] = WIFI_PASSWORD;
  doc["MQTT_SERVER"] = MQTT_SERVER;
  doc["MQTT_PORT"] = MQTT_PORT;
  doc["MQTT_CLIENT_ID"] = MQTT_CLIENT_ID;
  doc["MQTT_BUTTON_TOPIC"] = MQTT_BUTTON_TOPIC;
  doc["MQTT_RFID_TOPIC"] = MQTT_RFID_TOPIC;
  doc["MQTT_RFID_WRITE_TOPIC"] = MQTT_RFID_WRITE_TOPIC;
  doc["MQTT_DEVICE_INFO_TOPIC"] = MQTT_DEVICE_INFO_TOPIC;
  doc["MQTT_RECONNECT_INTERVAL"] = MQTT_RECONNECT_INTERVAL;
  doc["WIFI_RECONNECT_INTERVAL"] = WIFI_RECONNECT_INTERVAL;
  doc["RFID_CHECK_INTERVAL"] = RFID_CHECK_INTERVAL;

  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }
  file.close();
  return true;
}

bool loadConfig() {
  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) {
    return false;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    file.close();
    return false;
  }

  strlcpy(WIFI_SSID, doc["WIFI_SSID"] | "your_wifi_ssid", sizeof(WIFI_SSID));
  strlcpy(WIFI_PASSWORD, doc["WIFI_PASSWORD"] | "your_wifi_password", sizeof(WIFI_PASSWORD));
  strlcpy(MQTT_SERVER, doc["MQTT_SERVER"] | "your_mqtt_broker", sizeof(MQTT_SERVER));
  MQTT_PORT = doc["MQTT_PORT"] | 1883;
  strlcpy(MQTT_CLIENT_ID, doc["MQTT_CLIENT_ID"] | "ESP8266_ButtonCounter", sizeof(MQTT_CLIENT_ID));
  strlcpy(MQTT_BUTTON_TOPIC, doc["MQTT_BUTTON_TOPIC"] | "button/count", sizeof(MQTT_BUTTON_TOPIC));
  strlcpy(MQTT_RFID_TOPIC, doc["MQTT_RFID_TOPIC"] | "rfid/tag", sizeof(MQTT_RFID_TOPIC));
  strlcpy(MQTT_RFID_WRITE_TOPIC, doc["MQTT_RFID_WRITE_TOPIC"] | "rfid/write", sizeof(MQTT_RFID_WRITE_TOPIC));
  strlcpy(MQTT_DEVICE_INFO_TOPIC, doc["MQTT_DEVICE_INFO_TOPIC"] | "device/info", sizeof(MQTT_DEVICE_INFO_TOPIC));
  MQTT_RECONNECT_INTERVAL = doc["MQTT_RECONNECT_INTERVAL"] | 5000;
  WIFI_RECONNECT_INTERVAL = doc["WIFI_RECONNECT_INTERVAL"] | 5000;
  RFID_CHECK_INTERVAL = doc["RFID_CHECK_INTERVAL"] | 5000;

  file.close();
  return true;
}

#endif