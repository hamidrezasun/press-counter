#include "shell.h"
#include "config.h"
#include "store.h"
#include "clock.h"
#include "session.h"
#include "press.h"
#include "temp.h"
#include "errors.h"
#include "events.h"
#include "link.h"
#include "nfc.h"
#include "io.h"
#include "outputs.h"
#include "sense.h"
#include "bootloader.h"

namespace {
char g_line[80];
uint8_t g_n = 0;

void help(Print& o) {
  o.println(F("commands: help status cfg set <key> <val> save defaults err clear relay <0|1> reset"));
  o.println(F("          out <1..4> <0|1|2> | tag write <text> | call <1|2> | ack | buzz | time | reboot | rom | link <payload>"));
}

void status(Print& o) {
  char b[160];
  const int16_t t = temp::tempC10();
  snprintf(b, sizeof(b), "'%s' fw %s  relay %u  strokes %lu  last %lu ms  active %u  temp %d.%d C  vdda %u mV",
           store::cfg.name, PC_FW_VERSION_STR, session::relayOn() ? 1 : 0, (unsigned long)press::count(),
           (unsigned long)press::lastDurationMs(), press::active() ? 1 : 0, t == INT16_MIN ? 0 : t / 10, t == INT16_MIN ? 0 : (t < 0 ? -t : t) % 10,
           temp::vddaMilliVolt());
  o.println(b);
  snprintf(b, sizeof(b), "operator '%s' die '%s'  errors 0x%04X  calls 0x%02X  queue %u  dropped %lu  lifetime %lu",
           session::operatorId(), session::dieId(), err::mask(), session::callMask(), events::pending(), (unsigned long)events::dropped(),
           (unsigned long)store::lifetime());
  o.println(b);
  snprintf(b, sizeof(b), "outputs 0x%02X (OUT1 = bit 0)  12 V input %u mV  230 V input %u.%u V", outputs::mask(), sense::v12mV(), sense::vac10() / 10, sense::vac10() % 10);
  o.println(b);
  snprintf(b, sizeof(b), "link %s wifi %s server %s ntp %s ip %s rssi %d  nfc %s",
           link::alive() ? "up" : "DOWN", link::wifiUp() ? "up" : "down", link::serverUp() ? "ok" : "no", link::ntpOk() ? "ok" : "no",
           link::ip(), link::rssi(), nfc::present() ? "ok" : "ABSENT");
  o.println(b);
}

void run(Print& o, char* cmd) {
  char* arg = strchr(cmd, ' ');
  if (arg) { *arg++ = '\0'; while (*arg == ' ') arg++; } else arg = cmd + strlen(cmd);
  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) help(o);
  else if (!strcmp(cmd, "status")) status(o);
  else if (!strcmp(cmd, "cfg")) {
    for (int i = 0; i < store::paramCount(); i++) { char v[40]; store::getParam(store::paramKey(i), v, sizeof(v)); o.print(store::paramKey(i)); o.print(F(" = ")); o.println(v); }
  } else if (!strcmp(cmd, "set")) {
    char* val = strchr(arg, ' ');
    if (!val) { o.println(F("usage: set <key> <value>")); return; }
    *val++ = '\0';
    o.println(store::setParam(arg, val) ? F("ok (use 'save' to keep it)") : F("rejected"));
  } else if (!strcmp(cmd, "save")) o.println(store::save() ? F("saved") : F("flash write failed"));
  else if (!strcmp(cmd, "defaults")) { store::defaults(); o.println(store::save() ? F("defaults saved") : F("flash write failed")); }
  else if (!strcmp(cmd, "err")) {
    for (uint8_t c = 1; c <= pc::ERR_LAST; c++) if (err::active(c)) { o.print('E'); o.print(c); o.print(' '); o.println(pc::errorText(c)); }
  } else if (!strcmp(cmd, "clear")) { err::acknowledge(); o.println(F("latched errors cleared")); }
  else if (!strcmp(cmd, "relay")) {
    if (!strcmp(arg, "1")) session::remoteCommand(pc::CMD_RELAY_ON, 0, pc::RR_REMOTE);
    else session::setRelay(false, pc::RR_REMOTE);
    o.println(session::relayOn() ? F("relay ON") : F("relay OFF"));
  } else if (!strcmp(cmd, "out")) {                                   // out <1..4> <0|1|2>
    const int idx = pc::toInt(arg);
    const char* sp = strchr(arg, ' ');
    if (idx < 1 || idx > pc::OUT_COUNT || !sp) o.println(F("usage: out <1..4> <0|1|2 toggle>"));
    else o.println(outputs::command((uint8_t)idx, (uint8_t)pc::toInt(sp + 1), pc::RR_REMOTE) ? F("ok") : F("refused (manual mode? remote start? power limit?)"));
  } else if (!strcmp(cmd, "reset")) { session::resetCounter(); o.println(F("counter reset")); }
  else if (!strcmp(cmd, "tag")) {
    if (!strncmp(arg, "write ", 6)) { nfc::queueWrite(arg + 6); o.println(F("present a card within 30 s")); }
    else { o.print(F("last tag: ")); o.println(nfc::lastTag()); }
  } else if (!strcmp(cmd, "call")) { session::call((uint8_t)pc::toInt(arg)); o.println(F("call raised")); }
  else if (!strcmp(cmd, "ack")) { session::ackCall(0); o.println(F("calls acknowledged")); }
  else if (!strcmp(cmd, "buzz")) { io::play(io::SND_SUCCESS); o.println(F("beep")); }
  else if (!strcmp(cmd, "time")) { o.println((unsigned long)clk::sec()); }
  else if (!strcmp(cmd, "reboot")) { o.println(F("rebooting")); delay(20); NVIC_SystemReset(); }
  else if (!strcmp(cmd, "rom")) { o.println(F("entering the STM32 ROM bootloader (USART1)")); delay(20); bootloader::enterRom(); }
  else if (!strcmp(cmd, "link")) { o.println(link::send(arg) ? F("queued") : F("tx buffer full")); }
  else if (cmd[0]) { o.println(F("unknown command, try 'help'")); }
}
}  // namespace

namespace shell {

void begin() {
#ifdef USBCON
  SerialUSB.begin();
#endif
}

void poll() {
#ifdef USBCON
  if (!SerialUSB) { g_n = 0; return; }                  // no terminal attached: never block on the USB port
  int guard = 64;
  while (guard-- && SerialUSB.available() > 0) {
    const int ch = SerialUSB.read();
    if (ch < 0) break;
    if (ch == '\r' || ch == '\n') {
      if (g_n) { g_line[g_n] = '\0'; g_n = 0; run(SerialUSB, g_line); }
    } else if ((size_t)g_n + 1 < sizeof(g_line)) g_line[g_n++] = (char)ch;
  }
#endif
}

}  // namespace shell
