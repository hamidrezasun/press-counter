// gw_settings.h - gateway settings stored as JSON in LittleFS (/config.json).  Secrets are never sent back to the web UI.
#ifndef GW_SETTINGS_H
#define GW_SETTINGS_H

#include <Arduino.h>

struct GwSettings {
  char wifiSsid[33];
  char wifiPass[65];
  char device[24];              // used until the STM32 reports its own name
  char mqttHost[64];
  uint16_t mqttPort;
  char mqttTopic[48];           // topic prefix, e.g. press/machine1  ->  press/machine1/<device>/event/press ...
  char mqttClient[32];
  char mqttUser[32];
  char mqttPass[48];
  char updateUrl[96];           // base URL of the firmware server (manifest.json lives there)
  char updateToken[64];         // sent as "Authorization: Bearer <token>"
  uint8_t autoUpdate;           // 1 = check the manifest by itself
  uint16_t checkMinutes;
  char ntpHost[48];
  char adminPass[32];           // if not empty: HTTP basic auth ("admin") for every command / settings change
  char otaPass[32];             // ArduinoOTA password
};

namespace settings {
extern GwSettings cfg;
bool begin();                   // mounts LittleFS, loads (or applies defaults)
bool save();
void defaults();
bool hasWifi();
}  // namespace settings

#endif
