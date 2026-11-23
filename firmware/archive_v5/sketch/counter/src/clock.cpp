#include "clock.h"

namespace {
const uint32_t PLAUSIBLE_AFTER = 1700000000UL;       // 2023-11-14
bool     g_valid = false;
uint32_t g_baseSec = 0;                              // unix time at g_baseMs
uint16_t g_baseMsPart = 0;
uint32_t g_baseMs = 0;
uint32_t g_syncMs = 0;

// move the base forward so that (millis() - g_baseMs) never gets near the 49 day wrap
void rebase(uint32_t nowMs) {
  uint32_t d = nowMs - g_baseMs;
  if (d < 1000UL * 3600UL) return;
  const uint32_t s = d / 1000UL;
  g_baseSec += s;
  g_baseMs += s * 1000UL;
}
}  // namespace

namespace clk {

void set(uint32_t sec, uint16_t ms) {
  if (sec < PLAUSIBLE_AFTER) return;
  g_baseSec = sec;
  g_baseMsPart = ms > 999 ? 999 : ms;
  g_baseMs = millis();
  g_syncMs = g_baseMs;
  g_valid = true;
}

bool valid() { return g_valid; }

void now(uint32_t& sec, uint16_t& ms) {
  if (!g_valid) { sec = 0; ms = 0; return; }
  const uint32_t t = millis();
  rebase(t);
  const uint32_t d = (t - g_baseMs) + g_baseMsPart;
  sec = g_baseSec + d / 1000UL;
  ms = (uint16_t)(d % 1000UL);
}

uint32_t sec() { uint32_t s; uint16_t m; now(s, m); return s; }

uint32_t ageMs() { return g_valid ? millis() - g_syncMs : 0xFFFFFFFFUL; }

}  // namespace clk
