// pc_net.h - WiFi (station + setup access point), NTP, mDNS, ArduinoOTA.
#ifndef PC_NET_H
#define PC_NET_H

#include "pc_config.h"

namespace net {
void begin();
void loop();
bool wifiUp();
bool apMode();                 // the setup access point is open
const char* apSsid();
const char* ip();              // station address ("" when not connected), 192.168.4.1 in setup mode without station
int rssi();
void startSetupAp();           // also from the two-button hold
void reconnect();              // new credentials were saved
const char* hostName();
}  // namespace net

#endif
