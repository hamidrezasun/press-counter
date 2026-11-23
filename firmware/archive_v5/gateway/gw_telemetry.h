// gw_telemetry.h - events and measurements from the STM32 -> MQTT (the only backend); MQTT commands -> STM32.
#ifndef GW_TELEMETRY_H
#define GW_TELEMETRY_H

#include <Arduino.h>
#include "gw_link.h"

namespace tele {
void begin();
void loop();
bool onEvent(const GwEvent& e);       // gwlink handler: true = accepted (published to the broker, or no broker configured)
void publishStatus();                 // retained status + voltage JSON on MQTT (call every few seconds)
void publishDeviceInfo();             // retained info JSON (ip, rssi, mac, versions)
bool serverOk();                      // MQTT configured and connected
bool mqttConfigured();
bool mqttOk();
void reconfigure();                   // settings changed: reconnect
}  // namespace tele

#endif
