// gw_web.h - HTTP interface of the gateway (port 80).
//
//   Compatible with the v2 firmware (the old dashboard / analyzer keep working):
//     GET /  /status  /press_counter  /save  /status/file        POST /save  /relay  /buzzer_error|warning|success  /write_tag  /upload
//   New JSON API used by the built-in page:
//     GET  /api/state  /api/settings  /api/log  /api/scan        POST /api/settings  /api/cmd  /api/node  /api/wifi  /api/update  /api/restart
//   Every POST (and GET /api/settings) needs HTTP basic auth ("admin" / adminPass) when an admin password is set.
#ifndef GW_WEB_H
#define GW_WEB_H

#include <Arduino.h>

namespace web {
void begin();
void loop();
bool takeWifiChange();        // /api/wifi stored new credentials: reconnect now
}  // namespace web

#endif
