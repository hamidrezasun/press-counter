// pc_store.h - persistent settings (machine + network) and the lifetime stroke counter, kept in the ESP32 NVS (Preferences).
#ifndef PC_STORE_H
#define PC_STORE_H

#include "pc_config.h"

// machine settings (validated table: setParam / getParam, also used by the web page, MQTT-free console and the defaults)
struct Settings {
  char     name[NAME_LEN];       // device name = MQTT topic part / WiFi host name
  uint16_t minPulseMs;           // a press signal shorter than this is noise
  uint16_t idleMin;              // machine is switched off after this many minutes without a stroke (0 = never)
  uint16_t stuckSec;             // E03 when the sense input stays active this long
  int16_t  tempOffset10;         // DS18B20 offset, 0.1 degC
  int16_t  overTemp10;           // E04 limit, 0.1 degC
  uint8_t  remoteStart;          // 1 = web / MQTT may switch the machine ON (stop is always allowed)
  uint8_t  sound;                // 0 off, 1 errors only, 2 all
  uint8_t  brightness;           // OLED contrast 5..100 %
  uint8_t  displayOn;            // 1 = an OLED module is fitted (E13 if it is missing)
  uint8_t  outMode[4];           // mode of OUT1..OUT4 (pc::OutMode)
  uint16_t v12Cal;               // calibration of the 12/24 V input, permille (1000 = nominal divider)
  uint16_t auxCal;               // calibration of the AUX input, permille
};

// network settings (web page "Settings" / setup access point).  Secrets are never sent back to the web page.
struct NetSettings {
  char wifiSsid[33];
  char wifiPass[65];
  char mqttHost[64];
  uint16_t mqttPort;
  char mqttTopic[48];            // topic prefix:  <prefix>/<device>/...
  char mqttClient[32];           // empty = the device name
  char mqttUser[32];
  char mqttPass[48];
  char updateUrl[96];            // folder of manifest.json on the user's firmware server
  char updateToken[64];          // "Authorization: Bearer <token>"
  uint8_t autoUpdate;
  uint16_t checkMinutes;
  char ntpHost[48];
  char adminPass[32];            // if set: HTTP basic auth ("admin") for every command / settings change
  char otaPass[32];              // ArduinoOTA password
};

namespace store {
extern Settings cfg;
extern NetSettings net;

bool begin();                                          // loads both (false -> machine settings were missing / invalid: defaults)
void defaults();
bool save();                                           // machine settings
void netDefaults();
bool saveNet();
bool setParam(const char* key, const char* value);     // validated; call save() afterwards
bool getParam(const char* key, char* out, size_t outSize);
int  paramCount();
const char* paramKey(int index);

uint32_t lifetime();                                   // strokes counted since the board was first programmed
void lifetimeAdd(uint32_t n);
void lifetimeFlush(bool force = false);                // writes to NVS every LIFETIME_SAVE_EVERY strokes (or force)
}  // namespace store

#endif
