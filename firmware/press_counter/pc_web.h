// pc_web.h - web page + JSON API (port 80).
//   GET  /  (built-in page; the WiFi form while the setup access point is the only network)
//   GET  /api/state  /api/settings  /api/log  /api/scan        POST /api/settings  /api/cmd  /api/wifi  /api/update  /api/restart  /write_tag
//   Kept from v2 for old dashboards: GET /status  /press_counter     POST /relay  /buzzer_error|warning|success
//   Every POST (and GET /api/settings, /api/scan) needs HTTP basic auth ("admin" / admin password) once an admin password is set.
#ifndef PC_WEB_H
#define PC_WEB_H

namespace web {
void begin();
void loop();
void pause();                          // stop serving (used while a firmware download owns the network)
void resume();                         // serve again
bool serving();
}  // namespace web

#endif
