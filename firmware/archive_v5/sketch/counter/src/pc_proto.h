// pc_proto.h - definitions shared by the counter (STM32) and the gateway (ESP8266) firmware.
// Plain C++11, no Arduino / HAL dependency, so it can also be compiled and tested on a PC (firmware/tests).
//
//   * error codes + texts            * remote command numbers
//   * UART link framing ($payload*HH) * time / number / text helpers
//   * CRC32 (firmware image check)
//
// The wire formats are documented in docs/PROTOCOL.md - keep both in sync.
#ifndef PC_PROTO_H
#define PC_PROTO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PC_FW_MAJOR 4
#define PC_FW_MINOR 0
#define PC_FW_VERSION_STR "4.0"
#define PC_FW_VERSION_U16 ((PC_FW_MAJOR << 8) | PC_FW_MINOR)

namespace pc {

// ── error codes ──────────────────────────────────────────────────────────────────────────────
// Shown as "Exx" on the display and sent to the server.  Severity: 0 info, 1 warning, 2 error.
enum ErrorCode : uint8_t {
  ERR_NONE         = 0,
  ERR_RFID         = 1,    // RC522 does not answer on SPI
  ERR_TEMP         = 2,    // LM35 open / shorted (ADC out of range)
  ERR_SENSE_STUCK  = 3,    // press sense input active for longer than the stuck limit
  ERR_OVERTEMP     = 4,    // temperature above the limit
  ERR_LINK         = 5,    // no answer from the WiFi gateway module (ESP8266)
  ERR_SERVER       = 6,    // WiFi down / server not reachable for a long time
  ERR_QUEUE        = 7,    // event queue overflowed, events were dropped
  ERR_SETTINGS     = 8,    // stored settings invalid, defaults loaded
  ERR_WATCHDOG     = 9,    // last reset was caused by the watchdog
  ERR_BYPASS       = 10,   // press stroke while the machine is switched off (contactor bypassed?)
  ERR_UPDATE       = 11,   // firmware update failed
  ERR_TAG_WRITE    = 12,   // writing a tag failed / timed out
  ERR_LAST         = 12
};

inline const char* errorText(uint8_t code) {
  static const char* const T[] = {
    "OK", "RFID READER", "TEMP SENSOR", "SENSE STUCK", "OVER TEMP", "GATEWAY LINK", "NO SERVER",
    "QUEUE FULL", "SETTINGS", "WATCHDOG", "STROKE LOCKED", "UPDATE FAILED", "TAG WRITE"};
  return code <= ERR_LAST ? T[code] : "UNKNOWN";
}
inline uint8_t errorSeverity(uint8_t code) {
  switch (code) {
    case ERR_NONE: return 0;
    case ERR_SERVER: case ERR_LINK: case ERR_QUEUE: case ERR_SETTINGS: case ERR_WATCHDOG:
    case ERR_UPDATE: case ERR_TAG_WRITE: return 1;
    default: return 2;                                   // RFID, TEMP, SENSE_STUCK, OVERTEMP, BYPASS
  }
}

// ── relay-change reasons ───────────────────────────────────────────────────────────────────────
enum RelayReason : uint8_t { RR_TAG = 0, RR_BUTTON = 1, RR_WEB = 2, RR_IDLE = 3, RR_REMOTE = 4, RR_BOOT = 5, RR_ERROR = 6, RR_AUTO = 7 };

// ── auxiliary output relays OUT1..OUT4 (K2..K5 on the main board) ──────────────────────────────────
static const uint8_t OUT_COUNT = 4;
static const uint8_t OUT_MAX_ON = 4;          // at most this many on at once (5 V module budget: K1 + 4 outputs = 0.37 A of coils)
// mode of an output (settings out1..out6): 0 manual (web / console / MQTT), 1 follows the machine relay K1, 2 on while any error or call is active,
// 3 on while the supervisor call is active, 4 on while the net / maintenance call is active, 5 on while the press sense input is active
enum OutMode : uint8_t { OM_MANUAL = 0, OM_MACHINE = 1, OM_ALERT = 2, OM_CALL_SUP = 3, OM_CALL_NET = 4, OM_PRESS = 5, OM_LAST = 5 };

// ── call types ─────────────────────────────────────────────────────────────────────────────────
enum CallType : uint8_t { CALL_NONE = 0, CALL_SUPERVISOR = 1, CALL_NET = 2 };

// ── NFC kinds ──────────────────────────────────────────────────────────────────────────────────
// 'P' personnel:<id> (toggles the relay)   'D' die:<id> (records the die only)   'U' anything else   'W' tag written
enum : char { NFC_PERSONNEL = 'P', NFC_DIE = 'D', NFC_UNKNOWN = 'U', NFC_WRITTEN = 'W' };

// ── commands from the network (web page / MQTT through the gateway, or the USB console) ───────────
enum Cmd : uint8_t {
  CMD_RELAY_OFF = 1,      // stop the machine (always allowed)
  CMD_RELAY_ON  = 2,      // start the machine (only if the settings allow remote start)
  CMD_RESET_CNT = 3,
  CMD_ACK_CALL  = 4,      // arg = call type (0 = all)
  CMD_BUZZ      = 5,      // arg = 0 success, 1 warning, 2 error
  CMD_REBOOT    = 6,
  CMD_CLEAR_ERR = 7,
  CMD_CALL      = 8       // arg = call type
};

// ── CRC32 (IEEE 802.3, same as zlib / Python binascii.crc32) - bitwise: tiny, speed is irrelevant here ─
inline uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320UL & (0u - (crc & 1u)));
  }
  return ~crc;
}
inline uint32_t crc32(const uint8_t* data, size_t len) { return crc32Update(0, data, len); }

// ── text helpers ─────────────────────────────────────────────────────────────────────────────────
// strlcpy replacement (not every libc has it): copies at most n-1 chars, always terminates.  Returns strlen(src).
inline size_t copyStr(char* dst, const char* src, size_t n) {
  size_t i = 0;
  if (n) { for (; src[i] && i + 1 < n; i++) dst[i] = src[i]; dst[i] = 0; }
  return strlen(src);
}
// Make a string safe for the line protocol: printable ASCII only, no ',', '*', '$'.  Returns dst.
inline char* sanitize(char* dst, size_t dstSize, const char* src) {
  size_t i = 0;
  if (dstSize == 0) return dst;
  for (; src && src[i] && i + 1 < dstSize; i++) {
    const unsigned char c = (unsigned char)src[i];
    dst[i] = (c < 0x20 || c > 0x7E || c == ',' || c == '*' || c == '$') ? '_' : (char)c;
  }
  dst[i] = '\0';
  return dst;
}

// "sec.mmm" <-> seconds + milliseconds.  sec == 0 means "time unknown".
inline int formatTime(char* out, size_t outSize, uint32_t sec, uint16_t ms) {
  // hand-rolled: newlib-nano printf on the STM32 has no 64 bit / float support and we want it tiny anyway
  char tmp[16];
  int n = 0;
  uint32_t v = sec;
  do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 11);
  size_t o = 0;
  while (n && o + 1 < outSize) out[o++] = tmp[--n];
  if (o + 5 >= outSize) { out[o < outSize ? o : outSize - 1] = '\0'; return (int)o; }
  out[o++] = '.';
  out[o++] = (char)('0' + (ms / 100) % 10);
  out[o++] = (char)('0' + (ms / 10) % 10);
  out[o++] = (char)('0' + ms % 10);
  out[o] = '\0';
  return (int)o;
}
inline bool parseTime(const char* s, uint32_t& sec, uint16_t& ms) {
  sec = 0; ms = 0;
  if (!s || !*s) return false;
  while (*s >= '0' && *s <= '9') sec = sec * 10 + (uint32_t)(*s++ - '0');
  if (*s == '.') {
    s++;
    uint16_t scale = 100;
    while (*s >= '0' && *s <= '9' && scale) { ms = (uint16_t)(ms + (*s++ - '0') * scale); scale = (uint16_t)(scale / 10); }
  }
  return true;
}

// unsigned / signed decimal parse of a field (no strtol: keeps the STM32 build small); returns false on garbage
inline bool parseInt(const char* s, int32_t& out) {
  if (!s || !*s) return false;
  bool neg = false;
  if (*s == '-') { neg = true; s++; }
  if (!*s) return false;
  int64_t v = 0;
  for (; *s; s++) {
    if (*s < '0' || *s > '9') return false;
    v = v * 10 + (*s - '0');
    if (v > 2147483647LL + 1) return false;
  }
  out = (int32_t)(neg ? -v : v);
  return true;
}
inline int32_t toInt(const char* s, int32_t def = 0) { int32_t v; return parseInt(s, v) ? v : def; }

// ── UART link framing:   $<payload>*HH\n      HH = XOR of all payload bytes, two upper-case hex digits ──────
static const size_t LINE_MAX = 200;           // payload bytes

inline uint8_t xor8(const char* p, size_t n) { uint8_t x = 0; while (n--) x ^= (uint8_t)*p++; return x; }

// Builds "$payload*HH\n" into out.  Returns the length, 0 if it does not fit.
inline size_t frameEncode(char* out, size_t outSize, const char* payload) {
  const size_t n = strlen(payload);
  if (n > LINE_MAX || outSize < n + 6) return 0;
  static const char HEXD[] = "0123456789ABCDEF";
  const uint8_t x = xor8(payload, n);
  out[0] = '$';
  memcpy(out + 1, payload, n);
  out[n + 1] = '*';
  out[n + 2] = HEXD[x >> 4];
  out[n + 3] = HEXD[x & 15];
  out[n + 4] = '\n';
  out[n + 5] = '\0';
  return n + 5;
}

// Byte-wise line parser.  feed() returns true when a complete, checksum-valid frame was received;
// payload() is then valid until the next feed().
class LineParser {
 public:
  bool feed(char c) {
    if (c == '$') { _n = 0; _in = true; return false; }
    if (!_in) return false;
    if (c == '\n' || c == '\r') {
      _in = false;
      if (_n < 3 || _buf[_n - 3] != '*') { _bad++; return false; }
      const size_t plen = _n - 3;
      const int hi = hexVal(_buf[_n - 2]), lo = hexVal(_buf[_n - 1]);
      if (hi < 0 || lo < 0 || (uint8_t)((hi << 4) | lo) != xor8(_buf, plen)) { _bad++; return false; }
      _buf[plen] = '\0';
      return true;
    }
    if (_n >= sizeof(_buf) - 1) { _in = false; _bad++; return false; }      // overlong: drop
    _buf[_n++] = c;
    return false;
  }
  char* payload() { return _buf; }
  uint32_t badFrames() const { return _bad; }
 private:
  static int hexVal(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
  char _buf[LINE_MAX + 8];
  size_t _n = 0;
  bool _in = false;
  uint32_t _bad = 0;
};

// Splits a payload in place at ',' (empty fields are kept).  Returns the number of fields (<= maxFields).
inline int splitFields(char* s, char** f, int maxFields) {
  int n = 0;
  if (maxFields <= 0) return 0;
  f[n++] = s;
  for (; *s && n < maxFields; s++) {
    if (*s == ',') { *s = '\0'; f[n++] = s + 1; }
  }
  return n;
}

}  // namespace pc

#endif  // PC_PROTO_H
