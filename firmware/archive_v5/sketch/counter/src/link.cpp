#include "link.h"
#include "store.h"
#include "clock.h"
#include "events.h"
#include "session.h"
#include "press.h"
#include "temp.h"
#include "nfc.h"
#include "errors.h"
#include "bootloader.h"
#include "io.h"
#include "outputs.h"
#include "sense.h"

namespace {
pc::LineParser g_parser;
char     g_tx[512];
uint16_t g_txHead = 0, g_txTail = 0;

bool     g_wifi = false, g_server = false, g_ntp = false;
int16_t  g_rssi = 0;
char     g_ip[20] = "";
uint8_t  g_fwState = 0, g_fwPct = 0;
uint32_t g_lastRx = 0, g_lastStatus = 0, g_lastIdent = 0;
uint32_t g_bad = 0;
uint32_t g_wifiLostSince = 0;

uint16_t txUsed() { return (uint16_t)((g_txHead + sizeof(g_tx) - g_txTail) % sizeof(g_tx)); }
uint16_t txFreeBytes() { return (uint16_t)(sizeof(g_tx) - 1 - txUsed()); }

void handle(char* payload) {
  char* f[10];
  const int n = pc::splitFields(payload, f, 10);
  if (n < 1 || !f[0][0]) return;
  const char c = f[0][0];
  g_lastRx = millis();
  switch (c) {
    case 'N': {                                   // N,wifi,server,rssi,ip,ntp,fwstate,fwpct
      if (n >= 6) {
        g_wifi = f[1][0] == '1';
        g_server = f[2][0] == '1';
        g_rssi = (int16_t)pc::toInt(f[3]);
        pc::copyStr(g_ip, f[4], sizeof(g_ip));
        g_ntp = f[5][0] == '1';
        if (n >= 7) g_fwState = (uint8_t)pc::toInt(f[6]);
        if (n >= 8) g_fwPct = (uint8_t)pc::toInt(f[7]);
      }
      break;
    }
    case 'T': {                                   // T,sec.ms
      uint32_t s; uint16_t ms;
      if (n >= 2 && pc::parseTime(f[1], s, ms)) clk::set(s, ms);
      break;
    }
    case 'A':                                     // A,seq
      if (n >= 2) events::onAck((uint16_t)pc::toInt(f[1]));
      break;
    case 'W':                                     // W,text   (write this text to the next card)
      if (n >= 2) { nfc::queueWrite(f[1]); session::banner("PRESENT CARD TO WRITE", 30000); }
      break;
    case 'K': {                                   // K,cmd[,arg]
      if (n < 2) break;
      const char* cmd = f[1];
      const char* arg = n >= 3 ? f[2] : "";
      if (!strcmp(cmd, "relay")) {
        if (arg[0] == '0') session::remoteCommand(pc::CMD_RELAY_OFF, 0, pc::RR_WEB);
        else if (arg[0] == '1') session::remoteCommand(pc::CMD_RELAY_ON, 0, pc::RR_WEB);
        else if (session::relayOn()) session::remoteCommand(pc::CMD_RELAY_OFF, 0, pc::RR_WEB);
        else session::remoteCommand(pc::CMD_RELAY_ON, 0, pc::RR_WEB);
      } else if (!strcmp(cmd, "reset")) session::remoteCommand(pc::CMD_RESET_CNT, 0, pc::RR_WEB);
      else if (!strcmp(cmd, "buzz")) session::remoteCommand(pc::CMD_BUZZ, !strcmp(arg, "success") ? 0 : !strcmp(arg, "warning") ? 1 : 2, pc::RR_WEB);
      else if (!strcmp(cmd, "call")) session::remoteCommand(pc::CMD_CALL, (uint8_t)pc::toInt(arg), pc::RR_WEB);
      else if (!strcmp(cmd, "ack")) session::ackCall(!strcmp(arg, "all") ? 0 : (uint8_t)pc::toInt(arg));
      else if (!strcmp(cmd, "out")) {            // K,out,<n>:<0|1|2>   (2 = toggle)
        const char* c = strchr(arg, ':');
        if (c) outputs::command((uint8_t)pc::toInt(arg), (uint8_t)pc::toInt(c + 1), pc::RR_WEB);
      }
      else if (!strcmp(cmd, "clr")) err::acknowledge();
      else if (!strcmp(cmd, "reboot")) session::remoteCommand(pc::CMD_REBOOT, 0, pc::RR_WEB);
      else if (!strcmp(cmd, "boot")) {            // the gateway wants to flash this chip: only when the machine is idle
        if (!session::relayOn() && !press::active()) { link::send("B,OK"); bootloader::enterRom(); }
        else link::send("B,BUSY");
      }
      break;
    }
    case 'G': link::sendAllSettings(); break;
    case 'S':                                     // S,key,value
      if (n >= 3) {
        const bool ok = store::setParam(f[1], f[2]) && store::save();
        if (ok) link::sendSetting(f[1]);
        else { char b[48]; snprintf(b, sizeof(b), "Q,%s,ERR", f[1]); link::send(b); }
      }
      break;
    case 'R': link::sendIdentity(); break;
    case 'P': link::send("Y"); break;
    default: break;
  }
}
}  // namespace

namespace link {

void begin() {
  LinkSerial.begin(LINK_BAUD);
  g_lastRx = 0;
}

bool send(const char* payload) {
  char frame[LINE_PAYLOAD + 8];
  const size_t n = pc::frameEncode(frame, sizeof(frame), payload);
  if (n == 0 || n > txFreeBytes()) return false;
  for (size_t i = 0; i < n; i++) { g_tx[g_txHead] = frame[i]; g_txHead = (uint16_t)((g_txHead + 1) % sizeof(g_tx)); }
  return true;
}

bool alive() { return g_lastRx != 0 && millis() - g_lastRx < LINK_ALIVE_MS; }
bool wifiUp() { return alive() && g_wifi; }
bool serverUp() { return alive() && g_wifi && g_server; }
bool ntpOk() { return alive() && g_ntp; }
bool gatewayReady() { return wifiUp(); }
int16_t rssi() { return g_rssi; }
const char* ip() { return g_ip; }
uint8_t fwState() { return alive() ? g_fwState : 0; }
uint8_t fwPercent() { return g_fwPct; }
uint32_t lastRxMs() { return g_lastRx; }
uint32_t badFrames() { return g_bad + g_parser.badFrames(); }

void sendIdentity() {
  char b[LINE_PAYLOAD];
  const uint8_t flags = bootloader::wasWatchdogReset() ? 4 : 0;                       // bit2: the last reset was the watchdog
  snprintf(b, sizeof(b), "I,%s,%s,%u", store::cfg.name, PC_FW_VERSION_STR, flags);
  send(b);
}

void sendSetting(const char* key) {
  char v[40], b[LINE_PAYLOAD];
  if (!store::getParam(key, v, sizeof(v))) return;
  snprintf(b, sizeof(b), "Q,%s,%s", key, v);
  send(b);
}

void sendAllSettings() {
  for (int i = 0; i < store::paramCount(); i++) sendSetting(store::paramKey(i));
}

// S,relay,count,lastDur,inPress,tempC10,topErr,operator,die,sessionSec,uptimeSec,queued,lastAgoSec,lifetime,errMask,calls,vdda,mcuTemp10,outs,v12mV,vac10
void sendStatus() {
  char op[MAX_TAG_TEXT + 2], die[MAX_TAG_TEXT + 2], b[LINE_PAYLOAD];
  pc::sanitize(op, sizeof(op), session::operatorId());
  pc::sanitize(die, sizeof(die), session::dieId());
  const uint32_t ago = press::sinceLastStrokeMs();
  const int16_t t = temp::tempC10();
  snprintf(b, sizeof(b), "S,%u,%lu,%lu,%u,%d,%u,%s,%s,%lu,%lu,%u,%ld,%lu,%u,%u,%u,%d,%u,%u,%u",
           session::relayOn() ? 1 : 0, (unsigned long)press::count(), (unsigned long)press::lastDurationMs(), press::active() ? 1 : 0,
           t == INT16_MIN ? 9999 : t, err::top(), op, die, (unsigned long)session::sessionSeconds(), (unsigned long)(millis() / 1000UL),
           events::pending(), ago == 0xFFFFFFFFUL ? -1L : (long)(ago / 1000UL),
           (unsigned long)store::lifetime(), err::mask(), session::callMask(), temp::vddaMilliVolt(), temp::mcuTempC10(), outputs::mask(), sense::v12mV(), sense::vac10());
  send(b);
}

void poll() {
  // receive
  int guard = 128;
  while (guard-- && LinkSerial.available() > 0) {
    const int c = LinkSerial.read();
    if (c < 0) break;
    if (g_parser.feed((char)c)) handle(g_parser.payload());
  }
  // transmit what the UART can take without blocking
  while (g_txTail != g_txHead && LinkSerial.availableForWrite() > 0) {
    LinkSerial.write((uint8_t)g_tx[g_txTail]);
    g_txTail = (uint16_t)((g_txTail + 1) % sizeof(g_tx));
  }
  const uint32_t now = millis();
  if (now - g_lastStatus >= STATUS_PERIOD_MS) { g_lastStatus = now; sendStatus(); }
  if (now - g_lastIdent >= 30000UL || (alive() && g_lastIdent == 0)) { g_lastIdent = now ? now : 1; sendIdentity(); }

  // gateway health -> errors
  static uint32_t linkLostSince = 0;
  if (alive()) linkLostSince = 0; else if (!linkLostSince) linkLostSince = now ? now : 1;
  err::set(pc::ERR_LINK, linkLostSince && now - linkLostSince > LINK_ERROR_MS);
  if (alive() && !g_wifi) { if (!g_wifiLostSince) g_wifiLostSince = now ? now : 1; } else g_wifiLostSince = 0;
  err::set(pc::ERR_SERVER, (g_wifiLostSince && now - g_wifiLostSince > SERVER_ERROR_MS));
}

}  // namespace link
