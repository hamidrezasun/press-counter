#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

class Config {
public:
  String ntp_server = "win-3d0te4stpjh.tooswasher.com";
  int debounce_delay = 50; // ms
  String device_id = "esp_press_1";
  String wifi_ssid = "TW factory";
  String wifi_password = "e8cac589tw";
  String mqtt_server = "192.168.1.37";
  int mqtt_port = 1883;
  String mqtt_counting_topic = "press/counting";
  String mqtt_nfc_topic = "press/nfc";
  String mqtt_device_info_topic = "press/device_info";
  String mqtt_turnon_topic = "press/turnon";
  String mqtt_button_topic = "press/button";
  String mqtt_username = "hamidreza";
  String mqtt_password = "faas20011";
  bool counting = false;
  bool load();
  bool save();
  void setCounting(bool state);
};

#endif