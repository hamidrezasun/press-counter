#include "pc_outputs.h"
#include "pc_errors.h"
#include "pc_events.h"
#include "pc_io.h"
#include "pc_press.h"
#include "pc_session.h"
#include "pc_store.h"

namespace {
const uint8_t OUT_PINS[pc::OUT_COUNT] = {PIN_OUT1, PIN_OUT2, PIN_OUT3, PIN_OUT4};
uint8_t g_manual = 0;        // requested state of the manual outputs
uint8_t g_on = 0;            // state of the pins
uint8_t g_lastReason = pc::RR_WEB;

uint8_t desired() {
  uint8_t want = 0;
  const uint8_t calls = session::callMask();
  for (uint8_t i = 0; i < pc::OUT_COUNT; i++) {
    bool on = false;
    switch (store::cfg.outMode[i]) {
      case pc::OM_MANUAL: on = (g_manual >> i) & 1; break;
      case pc::OM_MACHINE: on = session::relayOn(); break;
      case pc::OM_ALERT: on = err::count() > 0 || calls != 0; break;
      case pc::OM_CALL_SUP: on = (calls >> pc::CALL_SUPERVISOR) & 1; break;
      case pc::OM_CALL_NET: on = (calls >> pc::CALL_NET) & 1; break;
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
  for (uint8_t i = 0; i < pc::OUT_COUNT; i++) io::outputLow(OUT_PINS[i]);   // never HIGH while starting (a 10 k resistor holds each base low before)
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
  g_lastReason = reason;
  poll();
  return true;
}

void poll() {
  const uint8_t want = desired();
  if (want == g_on) return;
  for (uint8_t i = 0; i < pc::OUT_COUNT; i++) {
    const uint8_t bit = (uint8_t)(1u << i);
    if ((want & bit) == (g_on & bit)) continue;
    digitalWrite(OUT_PINS[i], (want & bit) ? HIGH : LOW);
    events::pushOutput((uint8_t)(i + 1), (want & bit) != 0, store::cfg.outMode[i] == pc::OM_MANUAL ? g_lastReason : (uint8_t)pc::RR_AUTO);
  }
  g_on = want;
}

}  // namespace outputs
