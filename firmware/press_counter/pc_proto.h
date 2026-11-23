// pc_proto.h - definitions of the press counter firmware that have no Arduino dependency (error codes, commands, helpers).
// Plain C++11, so it can also be compiled and tested on a PC (firmware/tests).
#ifndef PC_PROTO_H
#define PC_PROTO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PC_FW_MAJOR 6
#define PC_FW_MINOR 2
#define PC_FW_VERSION_STR "6.2"

namespace pc {

// ── error codes ──────────────────────────────────────────────────────────────────────────────
// Shown as "Exx" on the display and sent on MQTT.  Severity: 0 info, 1 warning, 2 error.  The numbers are kept from v5 (5 is unused now).
enum ErrorCode : uint8_t {
  ERR_NONE         = 0,
  ERR_RFID         = 1,    // RC522 does not answer on SPI
  ERR_TEMP         = 2,    // DS18B20 missing / CRC errors
  ERR_SENSE_STUCK  = 3,    // press sense input active for longer than the stuck limit
  ERR_OVERTEMP     = 4,    // temperature above the limit
  ERR_UNUSED5      = 5,    // (v5: gateway link)
  ERR_SERVER       = 6,    // WiFi / MQTT not reachable for a long time
  ERR_QUEUE        = 7,    // event queue overflowed, events were dropped
  ERR_SETTINGS     = 8,    // stored settings invalid, defaults loaded
  ERR_WATCHDOG     = 9,    // last reset was caused by a watchdog / crash
  ERR_BYPASS       = 10,   // press stroke while the machine is switched off (contactor bypassed?)
  ERR_UPDATE       = 11,   // firmware update failed
  ERR_TAG_WRITE    = 12,   // writing a tag failed / timed out
  ERR_DISPLAY      = 13,   // display enabled in the settings but not found on I2C
  ERR_LAST         = 13
};

inline const char* errorText(uint8_t code) {
  static const char* const T[] = {
    "OK", "RFID READER", "TEMP SENSOR", "SENSE STUCK", "OVER TEMP", "-", "NO SERVER",
    "QUEUE FULL", "SETTINGS", "WATCHDOG", "STROKE LOCKED", "UPDATE FAILED", "TAG WRITE", "DISPLAY"};
  return code <= ERR_LAST ? T[code] : "UNKNOWN";
}
inline uint8_t errorSeverity(uint8_t code) {
  switch (code) {
    case ERR_NONE: case ERR_UNUSED5: return 0;
    case ERR_SERVER: case ERR_QUEUE: case ERR_SETTINGS: case ERR_WATCHDOG:
    case ERR_UPDATE: case ERR_TAG_WRITE: case ERR_DISPLAY: return 1;
    default: return 2;                                   // RFID, TEMP, SENSE_STUCK, OVERTEMP, BYPASS
  }
}

// ── relay-change reasons ───────────────────────────────────────────────────────────────────────
enum RelayReason : uint8_t { RR_TAG = 0, RR_BUTTON = 1, RR_WEB = 2, RR_IDLE = 3, RR_REMOTE = 4, RR_BOOT = 5, RR_ERROR = 6, RR_AUTO = 7 };

// ── output relays OUT1..OUT4 (K2..K5).  Mode (settings out1..out4): 0 manual (web / MQTT / console), 1 follows the machine relay K1,
//    2 on while any error or call is active, 3 on while the supervisor call is open, 4 on while the net call is open, 5 on while the press sense input is active
static const uint8_t OUT_COUNT = 4;
enum OutMode : uint8_t { OM_MANUAL = 0, OM_MACHINE = 1, OM_ALERT = 2, OM_CALL_SUP = 3, OM_CALL_NET = 4, OM_PRESS = 5, OM_LAST = 5 };

// ── call types ─────────────────────────────────────────────────────────────────────────────────
enum CallType : uint8_t { CALL_NONE = 0, CALL_SUPERVISOR = 1, CALL_NET = 2 };

// ── NFC kinds ──────────────────────────────────────────────────────────────────────────────────
// 'P' personnel:<id> (toggles the relay)   'D' die:<id> (records the die only)   'U' anything else   'W' tag written
enum : char { NFC_PERSONNEL = 'P', NFC_DIE = 'D', NFC_UNKNOWN = 'U', NFC_WRITTEN = 'W' };

// ── commands (web page / MQTT / console) ────────────────────────────────────────────────────────
enum Cmd : uint8_t {
  CMD_RELAY_OFF = 1,      // stop the machine (always allowed)
  CMD_RELAY_ON  = 2,      // start the machine (only if the settings allow remote start)
  CMD_RESET_CNT = 3,
  CMD_ACK_CALL  = 4,      // arg = call type (0 = all)
  CMD_BUZZ      = 5,      // arg = 0 success, 1 warning, 2 error
  CMD_REBOOT    = 6,
  CMD_CLEAR_ERR = 7,
  CMD_CALL      = 8,      // arg = call type
  CMD_RELAY_TOGGLE = 9
};

// ── text helpers ─────────────────────────────────────────────────────────────────────────────────
// strlcpy replacement: copies at most n-1 chars, always terminates.  Returns strlen(src).
inline size_t copyStr(char* dst, const char* src, size_t n) {
  size_t i = 0;
  if (n) { for (; src[i] && i + 1 < n; i++) dst[i] = src[i]; dst[i] = 0; }
  return strlen(src);
}
// Printable ASCII only, no ',', '*', '$' (names end up in MQTT topics, tag texts in JSON).  Returns dst.
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

// signed decimal parse of a field; returns false on garbage
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

// Dallas / Maxim 1-wire CRC8 (DS18B20 scratchpad)
inline uint8_t crc8Maxim(const uint8_t* p, size_t n) {
  uint8_t crc = 0;
  while (n--) {
    uint8_t b = *p++;
    for (int i = 0; i < 8; i++) {
      const uint8_t mix = (crc ^ b) & 1;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      b >>= 1;
    }
  }
  return crc;
}

// DS18B20 scratchpad bytes 0 / 1 -> 0.1 degC (12 bit, 1/16 degC per LSB), rounded
inline int16_t ds18b20Tenths(uint8_t lsb, uint8_t msb) {
  const int16_t raw = (int16_t)((uint16_t)msb << 8 | lsb);
  const int32_t t = (int32_t)raw * 10;
  return (int16_t)(t >= 0 ? (t + 8) / 16 : (t - 8) / 16);
}

}  // namespace pc

#endif  // PC_PROTO_H
