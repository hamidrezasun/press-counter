#include "pc_temp.h"
#include "pc_errors.h"
#include "pc_store.h"
#include "pc_log.h"

namespace {
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
int16_t  g_t10 = INT16_MIN;
uint32_t g_okAt = 0, g_startAt = 0, g_crcErr = 0;
bool     g_converting = false;
uint8_t  g_fails = 0;

// open-drain style: output LOW pulls the bus, INPUT releases it (4.7k pull-up on the board)
inline void busLow() { pinMode(PIN_TEMP, OUTPUT); digitalWrite(PIN_TEMP, LOW); }
inline void busRelease() { pinMode(PIN_TEMP, INPUT); }

bool reset() {                                   // true = a device answered with a presence pulse
  busLow();
  delayMicroseconds(480);
  portENTER_CRITICAL(&g_mux);
  busRelease();
  delayMicroseconds(70);
  const bool presence = digitalRead(PIN_TEMP) == LOW;
  portEXIT_CRITICAL(&g_mux);
  delayMicroseconds(410);
  return presence;
}

void writeBit(bool one) {
  portENTER_CRITICAL(&g_mux);
  busLow();
  delayMicroseconds(one ? 6 : 60);
  busRelease();
  portEXIT_CRITICAL(&g_mux);
  delayMicroseconds(one ? 64 : 10);
}

bool readBit() {
  portENTER_CRITICAL(&g_mux);
  busLow();
  delayMicroseconds(3);
  busRelease();
  delayMicroseconds(10);
  const bool b = digitalRead(PIN_TEMP) == HIGH;
  portEXIT_CRITICAL(&g_mux);
  delayMicroseconds(53);
  return b;
}

void writeByte(uint8_t v) { for (int i = 0; i < 8; i++) { writeBit(v & 1); v >>= 1; } }
uint8_t readByte() { uint8_t v = 0; for (int i = 0; i < 8; i++) if (readBit()) v |= (uint8_t)(1u << i); return v; }

// One-shot diagnostic: what is actually on the 1-Wire bus (PIN_TEMP).  A DS18B20 in a 3-wire setup
// answers a reset with a presence pulse and returns its 64-bit ROM code after 0x33 (READ ROM).
// No presence pulse = wrong pin / no sensor / no 3V3 / no pull-up.  Presence but a wrong CRC on the
// scratchpad = damaged sensor, missing common ground, too long or unshielded wiring, wrong pull-up.
void scanBus() {
  if (!reset()) { plog::log("temp: no presence pulse on GPIO%d (no sensor / no 3V3 / no pull-up)", (int)PIN_TEMP); return; }
  writeByte(0x33);                                           // READ ROM (only valid with one device on the bus)
  char rom[24];
  int n = 0;
  for (int i = 0; i < 8; i++) n += snprintf(rom + n, sizeof(rom) - n, "%02X", readByte());
  plog::log("temp: 1-Wire device ROM %s (crc errors so far %lu)", rom, (unsigned long)g_crcErr);
}

void fail(const char* why) {
  if (g_fails < 255) g_fails++;
  if (g_fails == 3) {
    g_t10 = INT16_MIN;
    err::set(pc::ERR_TEMP, true);
    plog::log("temp: %s - E02 (three failed conversions)", why);
    scanBus();
  }
}
}  // namespace

namespace temp {

void begin() {
  busRelease();
  g_startAt = millis() - TEMP_SAMPLE_MS;
}

void poll() {
  const uint32_t now = millis();
  if (g_converting) {
    if (now - g_startAt < 800UL) return;                     // 12 bit: 750 ms
    g_converting = false;
    if (!reset()) { fail("no presence pulse"); return; }
    writeByte(0xCC);                                         // skip ROM (one sensor on the bus)
    writeByte(0xBE);                                         // read scratchpad
    uint8_t sp[9];
    for (int i = 0; i < 9; i++) sp[i] = readByte();
    bool allFF = true;
    for (int i = 0; i < 9; i++) if (sp[i] != 0xFF) allFF = false;
    if (allFF || pc::crc8Maxim(sp, 8) != sp[8]) { g_crcErr++; fail(allFF ? "scratchpad all 0xFF" : "scratchpad CRC error"); return; }
    const int16_t t = pc::ds18b20Tenths(sp[0], sp[1]);
    if (t == 850 && g_okAt == 0) return;                     // +85.0 = power-on value before the first conversion: ignore once
    g_t10 = (int16_t)(t + store::cfg.tempOffset10);
    g_okAt = now ? now : 1;
    g_fails = 0;
    err::set(pc::ERR_TEMP, false);
    err::set(pc::ERR_OVERTEMP, g_t10 >= store::cfg.overTemp10, (uint16_t)g_t10);
    return;
  }
  if (now - g_startAt < TEMP_SAMPLE_MS) return;
  g_startAt = now;
  if (!reset()) { fail("no presence pulse"); return; }
  writeByte(0xCC);
  writeByte(0x44);                                           // convert T (the sensor is powered from 3V3, not parasitic)
  g_converting = true;
}

bool valid() { return g_t10 != INT16_MIN && g_okAt && millis() - g_okAt < 10000UL; }
int16_t tempC10() { return valid() ? g_t10 : INT16_MIN; }
uint32_t crcErrors() { return g_crcErr; }

}  // namespace temp
