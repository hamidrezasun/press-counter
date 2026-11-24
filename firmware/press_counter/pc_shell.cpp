#include "pc_shell.h"
#include "pc_config.h"
#include "pc_errors.h"
#include "pc_events.h"
#include "pc_io.h"
#include "pc_log.h"
#include "pc_mqtt.h"
#include "pc_net.h"
#include "pc_nfc.h"
#include "pc_outputs.h"
#include "pc_press.h"
#include "pc_sense.h"
#include "pc_session.h"
#include "pc_store.h"
#include "pc_temp.h"

namespace {
char g_line[96];
uint8_t g_n = 0;

void help(Print& o) {
  o.println(F("commands: help status cfg set <key> <val> save defaults err clear relay <0|1> out <1..4> <0|1|2> reset call <1|2> ack buzz"));
  o.println(F("          tag [write <text>] | wifi <ssid> <password> | mqtt <host> [port] | setup (WiFi access point) | log | reboot"));
}

void status(Print& o) {
  char b[200];
  const int16_t t = temp::tempC10();
  snprintf(b, sizeof(b), "'%s' fw %s  relay %u  strokes %lu  last %lu ms  active %u  temp %s%d.%d C  lifetime %lu",
           store::cfg.name, PC_FW_VERSION_STR, session::relayOn() ? 1 : 0, (unsigned long)press::count(), (unsigned long)press::lastDurationMs(),
           press::active() ? 1 : 0, (t != INT16_MIN && t < 0) ? "-" : "", t == INT16_MIN ? 0 : abs(t) / 10, t == INT16_MIN ? 0 : abs(t) % 10,
           (unsigned long)store::lifetime());
  o.println(b);
  snprintf(b, sizeof(b), "operator '%s' die '%s'  errors 0x%04X  calls 0x%02X  queue %u  dropped %lu  nfc %s",
           session::operatorId(), session::dieId(), err::mask(), session::callMask(), events::pending(), (unsigned long)events::dropped(),
           nfc::present() ? "ok" : "ABSENT");
  o.println(b);
  const uint16_t v12 = sense::v12mV(), aux = sense::auxmV();
  snprintf(b, sizeof(b), "outputs 0x%X (OUT1 = bit 0)  12/24V input %u.%02u V  AUX %u.%02u V",
           outputs::mask(), v12 / 1000, (v12 % 1000) / 10, aux / 1000, (aux % 1000) / 10);
  o.println(b);
  snprintf(b, sizeof(b), "wifi %s (%s, %d dBm)  ip %s  setup-AP %s  mqtt %s  ntp %s  heap %lu",
           net::wifiUp() ? "up" : "down", store::net.wifiSsid, net::rssi(), net::ip(), net::apMode() ? net::apSsid() : "off",
           mqtt::connected() ? "connected" : (mqtt::configured() ? "DOWN" : "not set"), clk::valid() ? "ok" : "no", (unsigned long)ESP.getFreeHeap());
  o.println(b);
}

void run(Print& o, char* cmd) {
  char* arg = strchr(cmd, ' ');
  if (arg) { *arg++ = '\0'; while (*arg == ' ') arg++; } else arg = cmd + strlen(cmd);
  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) help(o);
  else if (!strcmp(cmd, "status")) status(o);
  else if (!strcmp(cmd, "cfg")) {
    for (int i = 0; i < store::paramCount(); i++) { char v[40]; store::getParam(store::paramKey(i), v, sizeof(v)); o.print(store::paramKey(i)); o.print(F(" = ")); o.println(v); }
    o.print(F("mqtt = ")); o.print(store::net.mqttHost); o.print(':'); o.print(store::net.mqttPort); o.print(F("  topic ")); o.println(store::net.mqttTopic);
  } else if (!strcmp(cmd, "set")) {
    char* val = strchr(arg, ' ');
    if (!val) { o.println(F("usage: set <key> <value>")); return; }
    *val++ = '\0';
    o.println(store::setParam(arg, val) ? F("ok (use 'save' to keep it)") : F("rejected"));
  } else if (!strcmp(cmd, "save")) { o.println(store::save() ? F("saved") : F("NVS write failed")); mqtt::reconfigure(); }
  else if (!strcmp(cmd, "defaults")) { store::defaults(); o.println(store::save() ? F("defaults saved") : F("NVS write failed")); }
  else if (!strcmp(cmd, "err")) {
    for (uint8_t c = 1; c <= pc::ERR_LAST; c++) if (err::active(c)) { o.print('E'); o.print(c); o.print(' '); o.println(pc::errorText(c)); }
  } else if (!strcmp(cmd, "clear")) { err::acknowledge(); o.println(F("latched errors cleared")); }
  else if (!strcmp(cmd, "relay")) {
    if (!strcmp(arg, "1")) session::remoteCommand(pc::CMD_RELAY_ON, 0, pc::RR_REMOTE);
    else session::setRelay(false, pc::RR_REMOTE);
    o.println(session::relayOn() ? F("relay ON") : F("relay OFF (ON needs 'set remote 1')"));
  } else if (!strcmp(cmd, "out")) {                                   // out <1..4> <0|1|2>
    const int idx = pc::toInt(arg);
    const char* sp = strchr(arg, ' ');
    if (idx < 1 || idx > pc::OUT_COUNT || !sp) o.println(F("usage: out <1..4> <0|1|2 toggle>"));
    else o.println(outputs::command((uint8_t)idx, (uint8_t)pc::toInt(sp + 1), pc::RR_REMOTE) ? F("ok") : F("refused (not manual mode, or ON needs 'set remote 1')"));
  } else if (!strcmp(cmd, "reset")) { session::resetCounter(); o.println(F("counter reset")); }
  else if (!strcmp(cmd, "tag")) {
    if (!strncmp(arg, "write ", 6)) { nfc::queueWrite(arg + 6); o.println(F("present a card within 30 s")); }
    else { o.print(F("last tag: ")); o.println(nfc::lastTag()); }
  } else if (!strcmp(cmd, "call")) { session::call((uint8_t)pc::toInt(arg)); o.println(F("call raised")); }
  else if (!strcmp(cmd, "ack")) { session::ackCall(0); o.println(F("calls acknowledged")); }
  else if (!strcmp(cmd, "buzz")) { io::play(io::SND_SUCCESS); o.println(F("beep")); }
  else if (!strcmp(cmd, "wifi")) {
    char* pass = strchr(arg, ' ');
    if (pass) *pass++ = '\0';
    if (!arg[0]) { o.println(F("usage: wifi <ssid> [password]  (no blanks in the SSID here: use the web page for those)")); return; }
    pc::copyStr(store::net.wifiSsid, arg, sizeof(store::net.wifiSsid));
    pc::copyStr(store::net.wifiPass, pass ? pass : "", sizeof(store::net.wifiPass));
    store::saveNet();
    net::reconnect();
    o.println(F("saved, connecting"));
  } else if (!strcmp(cmd, "mqtt")) {
    char* port = strchr(arg, ' ');
    if (port) *port++ = '\0';
    pc::copyStr(store::net.mqttHost, arg, sizeof(store::net.mqttHost));
    if (port) store::net.mqttPort = (uint16_t)pc::toInt(port, 1883);
    store::saveNet();
    mqtt::reconfigure();
    o.println(F("saved"));
  } else if (!strcmp(cmd, "setup")) { net::startSetupAp(); o.println(F("setup access point open")); }
  else if (!strcmp(cmd, "log")) { for (int i = 0; i < plog::count(); i++) o.println(plog::line(i)); }
  else if (!strcmp(cmd, "reboot")) { o.println(F("rebooting")); session::remoteCommand(pc::CMD_REBOOT, 0, pc::RR_REMOTE); }
  else if (cmd[0]) { o.println(F("unknown command, try 'help'")); }
}
}  // namespace

namespace shell {

void begin() {
#ifndef PC_NO_CONSOLE
  Serial.begin(CONSOLE_BAUD);
#endif
}

void poll() {
#ifdef PC_NO_CONSOLE
  return;
#endif
  int guard = 64;
  while (guard-- && Serial.available() > 0) {
    const int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\r' || ch == '\n') {
      if (g_n) { g_line[g_n] = '\0'; g_n = 0; run(Serial, g_line); }
    } else if ((size_t)g_n + 1 < sizeof(g_line)) g_line[g_n++] = (char)ch;
  }
}

}  // namespace shell
