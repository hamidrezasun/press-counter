// gw_state.h - small shared state of the gateway: log ring buffer, last tag, network flags.
#ifndef GW_STATE_H
#define GW_STATE_H

#include <Arduino.h>
#include "gw_config.h"

namespace gws {
extern char lastTag[48];
extern bool wifiUp;
extern bool ntpOk;
extern bool apMode;                       // provisioning access point is active

const char* deviceName();                // the counter's own name, else the name stored in the gateway settings
void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
int logCount();
const char* logLine(int i);               // 0 = oldest
}  // namespace gws

#endif
