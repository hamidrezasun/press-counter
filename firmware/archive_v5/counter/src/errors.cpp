#include "errors.h"
#include "events.h"
#include "io.h"

namespace {
uint16_t g_mask = 0, g_latched = 0;
uint16_t g_detail[pc::ERR_LAST + 1];
uint32_t g_until[pc::ERR_LAST + 1];                 // millis() deadline, 0 = none

inline bool valid(uint8_t c) { return c >= 1 && c <= pc::ERR_LAST; }

void raiseNow(uint8_t code, uint16_t detail) {
  g_mask |= (uint16_t)(1u << code);
  g_detail[code] = detail;
  events::pushError(code, true, detail);
  if (pc::errorSeverity(code) >= 2) io::play(io::SND_ERROR);
  else io::play(io::SND_WARNING);
}
void clearNow(uint8_t code) {
  g_mask &= (uint16_t)~(1u << code);
  g_latched &= (uint16_t)~(1u << code);
  g_until[code] = 0;
  events::pushError(code, false, g_detail[code]);
}
}  // namespace

namespace err {

void begin() {
  g_mask = g_latched = 0;
  memset(g_detail, 0, sizeof(g_detail));
  memset(g_until, 0, sizeof(g_until));
}

void set(uint8_t code, bool on, uint16_t detail) {
  if (!valid(code)) return;
  const bool is = g_mask & (1u << code);
  if (on && !is) raiseNow(code, detail);
  else if (!on && is && !(g_latched & (1u << code))) clearNow(code);
}

void latch(uint8_t code, uint16_t detail, uint32_t ttlMs) {
  if (!valid(code)) return;
  if (!(g_mask & (1u << code))) raiseNow(code, detail);
  g_latched |= (uint16_t)(1u << code);
  g_until[code] = ttlMs ? (millis() + ttlMs) | 1u : 0;
}

void clear(uint8_t code) {
  if (valid(code) && (g_mask & (1u << code))) clearNow(code);
}

void acknowledge() {
  for (uint8_t c = 1; c <= pc::ERR_LAST; c++)
    if (g_latched & (1u << c)) clearNow(c);
}

void poll() {
  const uint32_t now = millis();
  for (uint8_t c = 1; c <= pc::ERR_LAST; c++)
    if (g_until[c] && (int32_t)(now - g_until[c]) >= 0 && (g_mask & (1u << c))) clearNow(c);
}

bool active(uint8_t code) { return valid(code) && (g_mask & (1u << code)); }
uint16_t mask() { return g_mask; }
uint16_t detail(uint8_t code) { return valid(code) ? g_detail[code] : 0; }

uint8_t count() {
  uint8_t n = 0;
  for (uint8_t c = 1; c <= pc::ERR_LAST; c++) if (g_mask & (1u << c)) n++;
  return n;
}

uint8_t top() {
  uint8_t best = 0, bestSev = 0;
  for (uint8_t c = 1; c <= pc::ERR_LAST; c++) {
    if (!(g_mask & (1u << c))) continue;
    const uint8_t sev = pc::errorSeverity(c);
    if (best == 0 || sev > bestSev) { best = c; bestSev = sev; }
  }
  return best;
}

}  // namespace err
