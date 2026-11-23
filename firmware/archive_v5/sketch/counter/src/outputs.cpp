#include "outputs.h"
#include "store.h"
#include "session.h"
#include "press.h"
#include "errors.h"
#include "events.h"
#include "io.h"

namespace {
const uint32_t OUT_PINS[pc::OUT_COUNT] = {PB11, PC13, PC14, PC15};
uint8_t g_manual = 0;        // requested state of the manual outputs
uint8_t g_on = 0;            // state of the pins
bool g_limitShown = false;

uint8_t desired() {
  uint8_t want = 0;
  for (uint8_t i = 0; i < pc::OUT_COUNT; i++) {
    bool on = false;
    switch (store::cfg.outMode[i]) {
      case pc::OM_MANUAL: on = (g_manual >> i) & 1; break;
      case pc::OM_MACHINE: on = session::relayOn(); break;
      case pc::OM_ALERT: on = err::count() > 0 || session::callMask() != 0; break;
      case pc::OM_CALL_SUP: on = (session::callMask() >> pc::CALL_SUPERVISOR) & 1; break;
      case pc::OM_CALL_NET: on = (session::callMask() >> pc::CALL_NET) & 1; break;
      case pc::OM_PRESS: on = press::active(); break;
      default: break;
    }
    if (on) want |= (uint8_t)(1u << i);
  }
  return want;
}
}  // namespace

namespace outputs {

void begin() {
  for (uint8_t i = 0; i < pc::OUT_COUNT; i++) {
    digitalWrite(OUT_PINS[i], LOW);                  // level first, then direction: no glitch (a 10 k resistor holds each base low while the MCU resets)
    pinMode(OUT_PINS[i], OUTPUT);
  }
  // PC13..PC15 are low-power pins (2 MHz, 3 mA): do not leave them at the core's 50 MHz default
  GPIOC->CRH = (GPIOC->CRH & ~0xFFF00000UL) | 0x22200000UL;
  g_manual = g_on = 0;
}

bool state(uint8_t index) { return index >= 1 && index <= pc::OUT_COUNT && ((g_on >> (index - 1)) & 1); }
uint8_t mask() { return g_on; }

bool command(uint8_t index, uint8_t st, uint8_t reason) {
  if (index < 1 || index > pc::OUT_COUNT) return false;
  const uint8_t bit = (uint8_t)(1u << (index - 1));
  if (store::cfg.outMode[index - 1] != pc::OM_MANUAL) { session::banner("OUTPUT IS AUTOMATIC"); return false; }
  const bool on = st == 2 ? !(g_manual & bit) : st == 1;
  if (on && !store::cfg.remoteStart) { session::banner("REMOTE START OFF"); io::play(io::SND_WARNING); return false; }
  if (on) g_manual |= bit; else g_manual &= (uint8_t)~bit;
  (void)reason;
  poll();
  return !on || (g_on & bit);
}

void poll() {
  uint8_t want = desired();
  // power budget: keep the lowest-numbered outputs, drop the rest
  uint8_t n = 0, keep = 0;
  for (uint8_t i = 0; i < pc::OUT_COUNT; i++) {
    if (!((want >> i) & 1)) continue;
    if (n < pc::OUT_MAX_ON) { keep |= (uint8_t)(1u << i); n++; }
  }
  if (keep != want) {
    if (!g_limitShown) { session::banner("OUTPUT LIMIT: 4 AT ONCE"); g_limitShown = true; }
  } else g_limitShown = false;
  want = keep;
  if (want == g_on) return;
  for (uint8_t i = 0; i < pc::OUT_COUNT; i++) {
    const uint8_t bit = (uint8_t)(1u << i);
    if ((want & bit) == (g_on & bit)) continue;
    digitalWrite(OUT_PINS[i], (want & bit) ? HIGH : LOW);
    events::pushOutput((uint8_t)(i + 1), (want & bit) != 0, store::cfg.outMode[i] == pc::OM_MANUAL ? pc::RR_WEB : pc::RR_AUTO);
  }
  g_on = want;
}

}  // namespace outputs
