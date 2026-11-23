// store.h - persistent settings (one flash page) and the lifetime stroke counter (log page).
#ifndef STORE_H
#define STORE_H

#include "config.h"

struct Settings {
  uint32_t magic;
  uint16_t version;
  uint16_t length;
  char     name[NAME_LEN];       // device name = Influx "device" tag / MQTT topic / WiFi host name
  uint16_t minPulseMs;           // a press signal shorter than this is noise
  uint16_t idleMin;              // machine is switched off after this many minutes without a stroke (0 = never)
  uint16_t stuckSec;             // E03 when the sense input stays active this long
  int16_t  tempOffset10;         // LM35 calibration, 0.1 degC
  int16_t  overTemp10;           // E04 limit, 0.1 degC
  uint8_t  manualToggle;         // 1 = the menu may switch the machine on/off without a card
  uint8_t  remoteStart;          // 1 = web / MQTT may switch the machine ON (stop is always allowed)
  uint8_t  sound;                // 0 off, 1 errors only, 2 all
  uint8_t  brightness;           // 5..100 %
  uint8_t  displayOn;            // 1 = a display module is fitted and used; 0 = run without display (everything stays available on the web page)
  uint8_t  outMode[4];           // mode of OUT1..OUT4 (pc::OutMode)
  uint8_t  reserved[1];
  uint16_t v12Cal;               // calibration of the 12 V input, permille (1000 = nominal divider)
  uint16_t acCal;                // calibration of the 230 V input (ZMPT101B module), permille
  uint32_t crc;                  // CRC32 of everything above
};

namespace store {
extern Settings cfg;

void defaults();
bool load();                                           // false -> defaults were applied (flash empty or damaged)
bool save();
bool setParam(const char* key, const char* value);     // validated; call save() afterwards
bool getParam(const char* key, char* out, size_t outSize);
int  paramCount();
const char* paramKey(int index);

uint32_t lifetime();                                   // strokes counted since the node was first programmed
void lifetimeAdd(uint32_t n);
void lifetimeFlush(bool force = false);                // writes to flash if enough strokes accumulated (or force)
}  // namespace store

#endif
