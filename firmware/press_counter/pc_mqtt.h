// pc_mqtt.h - MQTT, the only data backend.
//   <prefix>/<device>/online                 retained "online" / last will "offline"
//   <prefix>/<device>/status                 retained JSON every 5 s (counter, relay, temperature, operator, die, calls, errors, ...)
//   <prefix>/<device>/info                   retained JSON (ip, rssi, mac, firmware), every 10 min
//   <prefix>/<device>/event/<press|nfc|relay|call|error|temperature>   JSON, one message per event (queued while the broker is away)
//   <prefix>/<device>/sensor/voltage         retained JSON {"v12":24.1,"aux":0.0} every 5 s (volts)
//   <prefix>/<device>/relay/<1..5>/state     retained "ON" / "OFF" (1 = K1 the machine relay, 2..5 = OUT1..OUT4)
//   <prefix>/<device>/relay/<1..5>/set       command ON / OFF / TOGGLE (or 1 / 0); ON needs "remote start" in the settings
//   <prefix>/<device>/cmd                    JSON {"cmd":"relay_on|relay_off|relay|out_on|out_off|reset|ack|buzz|reboot|clear_errors|call|write_tag",..}
#ifndef PC_MQTT_H
#define PC_MQTT_H

#include "pc_config.h"

namespace mqtt {
void begin();
void loop();
bool configured();            // a broker host is set
bool connected();
bool publish(const char* sub, const char* payload, bool retain);     // <prefix>/<device>/<sub>
void publishStatus();
void publishInfo();
void reconfigure();           // settings changed: reconnect
}  // namespace mqtt

#endif
