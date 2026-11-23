#include "gw_link.h"
#include <sys/time.h>

namespace {
pc::LineParser g_parser;
StmStatus   g_status;
StmIdentity g_ident;
gwlink::EventHandler g_handler = nullptr;
uint32_t g_lastRx = 0, g_events = 0;
bool     g_paused = false;
bool     g_updReq = false;
struct Setting { char key[12]; char val[24]; bool used; };
Setting  g_settings[24];

void putSetting(const char* k, const char* v) {
  int freeSlot = -1;
  for (int i = 0; i < 24; i++) {
    if (g_settings[i].used && !strcmp(g_settings[i].key, k)) { pc::copyStr(g_settings[i].val, v, sizeof(g_settings[i].val)); return; }
    if (!g_settings[i].used && freeSlot < 0) freeSlot = i;
  }
  if (freeSlot >= 0) {
    g_settings[freeSlot].used = true;
    pc::copyStr(g_settings[freeSlot].key, k, sizeof(g_settings[freeSlot].key));
    pc::copyStr(g_settings[freeSlot].val, v, sizeof(g_settings[freeSlot].val));
  }
}

void parseEvent(char type, char** f, int n) {
  GwEvent e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  if (n < 2) return;
  e.seq = (uint16_t)pc::toInt(f[1]);
  // field positions differ per type (docs/PROTOCOL.md)
  switch (type) {
    case 'P':   // P,seq,count,dur,tempC10,relay,t
      if (n < 7) return;
      e.a = pc::toInt(f[2]); e.b = pc::toInt(f[3]); e.c = pc::toInt(f[4]); e.d = (uint16_t)pc::toInt(f[5]);
      pc::parseTime(f[6], e.tsec, e.tms);
      break;
    case 'N':   // N,seq,kind,relay,t,text
      if (n < 5) return;
      e.a = f[2][0]; e.d = (uint16_t)pc::toInt(f[3]);
      pc::parseTime(f[4], e.tsec, e.tms);
      if (n >= 6) pc::copyStr(e.text, f[5], sizeof(e.text));
      break;
    case 'R':   // R,seq,relay,reason,t
    case 'C':   // C,seq,type,state,t
      if (n < 5) return;
      e.a = pc::toInt(f[2]); e.b = pc::toInt(f[3]);
      pc::parseTime(f[4], e.tsec, e.tms);
      break;
    case 'O':   // O,seq,n,state,reason,t
      if (n < 6) return;
      e.a = pc::toInt(f[2]); e.b = pc::toInt(f[3]); e.d = (uint16_t)pc::toInt(f[4]);
      pc::parseTime(f[5], e.tsec, e.tms);
      break;
    case 'E':   // E,seq,code,state,detail,t
      if (n < 6) return;
      e.a = pc::toInt(f[2]); e.b = pc::toInt(f[3]); e.d = (uint16_t)pc::toInt(f[4]);
      pc::parseTime(f[5], e.tsec, e.tms);
      break;
    case 'T':   // T,seq,tempC10,t
      if (n < 4) return;
      e.c = pc::toInt(f[2]);
      pc::parseTime(f[3], e.tsec, e.tms);
      break;
    default: return;
  }
  g_events++;
  bool ok = true;
  if (g_handler) ok = g_handler(e);
  if (ok) {
    char b[24];
    snprintf(b, sizeof(b), "A,%u", e.seq);
    gwlink::send(b);
  }
}

void handleLine(char* payload) {
  char* f[24];
  const int n = pc::splitFields(payload, f, 24);
  if (n < 1 || !f[0][0]) return;
  g_lastRx = millis();
  const char c = f[0][0];
  switch (c) {
    case 'I':   // I,name,ver,flags
      if (n >= 4) {
        g_ident.valid = true;
        pc::copyStr(g_ident.name, f[1], sizeof(g_ident.name));
        pc::copyStr(g_ident.ver, f[2], sizeof(g_ident.ver));
        g_ident.flags = (uint8_t)pc::toInt(f[3]);
      }
      break;
    case 'S':   // S,relay,count,lastDur,inPress,tempC10,topErr,op,die,sessSec,uptime,queued,lastAgoS,lifetime,errMask,calls,vdda,mcuT10
      if (n >= 18) {
        StmStatus& s = g_status;
        s.valid = true;
        s.at = millis();
        s.relay = (uint8_t)pc::toInt(f[1]);
        s.count = (uint32_t)pc::toInt(f[2]);
        s.lastDur = (uint32_t)pc::toInt(f[3]);
        s.inPress = (uint8_t)pc::toInt(f[4]);
        s.tempC10 = pc::toInt(f[5]);
        s.topErr = (uint8_t)pc::toInt(f[6]);
        pc::copyStr(s.op, f[7], sizeof(s.op));
        pc::copyStr(s.die, f[8], sizeof(s.die));
        s.sessSec = (uint32_t)pc::toInt(f[9]);
        s.uptime = (uint32_t)pc::toInt(f[10]);
        s.queued = (uint8_t)pc::toInt(f[11]);
        s.lastAgoS = pc::toInt(f[12]);
        s.lifetime = (uint32_t)pc::toInt(f[13]);
        s.errMask = (uint16_t)pc::toInt(f[14]);
        s.calls = (uint8_t)pc::toInt(f[15]);
        s.vdda = (uint16_t)pc::toInt(f[16]);
        s.mcuT10 = pc::toInt(f[17]);
        s.outs = n >= 19 ? (uint8_t)pc::toInt(f[18]) : 0;
        s.v12mV = n >= 21 ? (uint16_t)pc::toInt(f[19]) : 0;
        s.vac10 = n >= 21 ? (uint16_t)pc::toInt(f[20]) : 0;
      }
      break;
    case 'Q': if (n >= 3) putSetting(f[1], f[2]); break;
    case 'P': case 'N': case 'R': case 'O': case 'C': case 'E': case 'T': parseEvent(c, f, n); break;
    case 'U': if (n >= 2 && !strcmp(f[1], "check")) g_updReq = true; break;   // the operator asked for an update check on the display
    default: break;                                // 'Y' pong, 'B' boot answers: nothing to do
  }
}
}  // namespace

namespace gwlink {

void begin() {
  Serial.setRxBufferSize(1024);
  Serial.begin(LINK_BAUD);
  Serial.swap();                                   // UART0 -> GPIO13 (RX) / GPIO15 (TX)
  pinMode(PIN_MCU_NRST, INPUT);
  pinMode(PIN_MCU_BOOT0, INPUT);
  memset(&g_status, 0, sizeof(g_status));
  memset(&g_ident, 0, sizeof(g_ident));
  memset(g_settings, 0, sizeof(g_settings));
  g_lastRx = 0;
}

void onEvent(EventHandler h) { g_handler = h; }
bool takeUpdateRequest() { const bool r = g_updReq; g_updReq = false; return r; }
bool alive() { return g_lastRx != 0 && millis() - g_lastRx < LINK_ALIVE_MS; }
const StmStatus& status() { return g_status; }
const StmIdentity& identity() { return g_ident; }
uint32_t eventsReceived() { return g_events; }
uint32_t badFrames() { return g_parser.badFrames(); }

bool send(const char* payload) {
  if (g_paused) return false;                     // the UART belongs to the STM32 ROM bootloader right now
  char frame[pc::LINE_MAX + 8];
  const size_t n = pc::frameEncode(frame, sizeof(frame), payload);
  if (!n) return false;
  Serial.write((const uint8_t*)frame, n);
  return true;
}

void sendNet(const char* ip, bool wifi, bool server, int rssi, bool ntp, uint8_t fwState, uint8_t fwPct) {
  char b[100];
  snprintf(b, sizeof(b), "N,%d,%d,%d,%s,%d,%u,%u", wifi ? 1 : 0, server ? 1 : 0, rssi, ip, ntp ? 1 : 0, fwState, fwPct);
  send(b);
}

void sendTime() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 1700000000L) return;
  char t[24], b[40];
  pc::formatTime(t, sizeof(t), (uint32_t)tv.tv_sec, (uint16_t)(tv.tv_usec / 1000));
  snprintf(b, sizeof(b), "T,%s", t);
  send(b);
}

void requestSettings() { send("G"); }

bool getSetting(const char* key, char* out, size_t n) {
  for (int i = 0; i < 24; i++)
    if (g_settings[i].used && !strcmp(g_settings[i].key, key)) { pc::copyStr(out, g_settings[i].val, n); return true; }
  return false;
}

bool setSetting(const char* key, const char* value) {
  char b[80];
  snprintf(b, sizeof(b), "S,%s,%s", key, value);
  return send(b);
}

void pause(bool on) { g_paused = on; }

void loop() {
  if (g_paused) return;
  int guard = 512;
  while (guard-- && Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) break;
    if (g_parser.feed((char)c)) handleLine(g_parser.payload());
  }
}

// ── STM32 ROM bootloader (AN3155) ───────────────────────────────────────────────────────────────────────────
namespace {
const uint8_t ACK = 0x79, NACK = 0x1F;
bool g_hasExtErase = false;

int rd(uint32_t timeoutMs) {
  const uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (Serial.available() > 0) return Serial.read();
    delay(0);
    yield();
  }
  return -1;
}
bool waitAck(uint32_t timeoutMs = 1000) { return rd(timeoutMs) == ACK; }
void flushRx() { while (Serial.available() > 0) Serial.read(); }
bool command(uint8_t cmd) {
  flushRx();
  Serial.write(cmd);
  Serial.write((uint8_t)(cmd ^ 0xFF));
  return waitAck();
}
bool sendAddress(uint32_t addr) {
  uint8_t b[5] = {(uint8_t)(addr >> 24), (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr, 0};
  b[4] = b[0] ^ b[1] ^ b[2] ^ b[3];
  Serial.write(b, 5);
  return waitAck();
}
void resetPulse() {
  pinMode(PIN_MCU_NRST, OUTPUT);
  digitalWrite(PIN_MCU_NRST, LOW);
  delay(60);
  pinMode(PIN_MCU_NRST, INPUT);
}
}  // namespace

bool enterBootloader() {
  pause(true);
  Serial.flush();
  Serial.end();
  Serial.setRxBufferSize(1024);
  Serial.begin(115200, SERIAL_8E1);
  Serial.swap();
  pinMode(PIN_MCU_BOOT0, OUTPUT);
  digitalWrite(PIN_MCU_BOOT0, HIGH);
  for (int attempt = 0; attempt < 4; attempt++) {
    resetPulse();
    delay(200);
    flushRx();
    for (int i = 0; i < 3; i++) {
      Serial.write((uint8_t)0x7F);
      if (waitAck(300)) {
        // read the supported command list to learn which erase command exists
        if (!command(0x00)) break;
        const int nBytes = rd(500);
        if (nBytes < 1) break;
        g_hasExtErase = false;
        int version = rd(200);
        (void)version;
        for (int k = 0; k < nBytes; k++) {
          const int cmd = rd(200);
          if (cmd == 0x44) g_hasExtErase = true;
        }
        return waitAck(500);
      }
    }
  }
  leaveBootloader();
  return false;
}

void leaveBootloader() {
  digitalWrite(PIN_MCU_BOOT0, LOW);
  pinMode(PIN_MCU_BOOT0, INPUT);
  resetPulse();
  Serial.flush();
  Serial.end();
  Serial.setRxBufferSize(1024);
  Serial.begin(LINK_BAUD);
  Serial.swap();
  g_lastRx = 0;
  pause(false);
}

bool stmErase(uint32_t addr, uint32_t size) {
  const uint32_t firstPage = (addr - STM32_FLASH_BASE) / 1024UL;
  const uint32_t pages = (size + 1023UL) / 1024UL;
  if (pages == 0 || pages > 120) return false;
  if (g_hasExtErase) {
    if (!command(0x44)) return false;
    uint8_t hdr[2] = {(uint8_t)((pages - 1) >> 8), (uint8_t)(pages - 1)};
    uint8_t chk = hdr[0] ^ hdr[1];
    Serial.write(hdr, 2);
    for (uint32_t p = 0; p < pages; p++) {
      const uint16_t pg = (uint16_t)(firstPage + p);
      const uint8_t b[2] = {(uint8_t)(pg >> 8), (uint8_t)pg};
      Serial.write(b, 2);
      chk ^= b[0] ^ b[1];
    }
    Serial.write(chk);
  } else {
    if (!command(0x43)) return false;
    const uint8_t cnt = (uint8_t)(pages - 1);
    uint8_t chk = cnt;
    Serial.write(cnt);
    for (uint32_t p = 0; p < pages; p++) {
      const uint8_t pg = (uint8_t)(firstPage + p);
      Serial.write(pg);
      chk ^= pg;
    }
    Serial.write(chk);
  }
  return waitAck(15000);
}

bool stmWrite(uint32_t addr, const uint8_t* data, size_t len) {
  if (len == 0 || len > 256 || (len & 3)) return false;
  if (!command(0x31)) return false;
  if (!sendAddress(addr)) return false;
  Serial.write((uint8_t)(len - 1));
  uint8_t chk = (uint8_t)(len - 1);
  for (size_t i = 0; i < len; i++) chk ^= data[i];
  Serial.write(data, len);
  Serial.write(chk);
  return waitAck(2000);
}

bool stmRead(uint32_t addr, uint8_t* out, size_t len) {
  if (len == 0 || len > 256) return false;
  if (!command(0x11)) return false;
  if (!sendAddress(addr)) return false;
  Serial.write((uint8_t)(len - 1));
  Serial.write((uint8_t)((len - 1) ^ 0xFF));
  if (!waitAck()) return false;
  for (size_t i = 0; i < len; i++) {
    const int c = rd(500);
    if (c < 0) return false;
    out[i] = (uint8_t)c;
  }
  return true;
}

}  // namespace gwlink
