#include "pc_buttons.h"
#include "pc_session.h"
#include "pc_io.h"

namespace {
struct Btn {
  uint8_t pin;
  uint8_t call;                        // pc::CallType
  bool raw = false, stable = false;
  uint32_t changedAt = 0, pressedAt = 0;
  bool longDone = false;
};
Btn g_b[2] = {{PIN_BTN_SUP, pc::CALL_SUPERVISOR}, {PIN_BTN_NET, pc::CALL_NET}};
uint32_t g_activity = 0;
bool g_setup = false, g_bothDone = false;
}  // namespace

namespace buttons {

void begin() {
  for (Btn& b : g_b) pinMode(b.pin, INPUT_PULLUP);      // 10k pull-up on the board as well
  g_activity = millis();
}

void poll() {
  const uint32_t now = millis();
  for (Btn& b : g_b) {
    const bool raw = digitalRead(b.pin) == LOW;
    if (raw != b.raw) { b.raw = raw; b.changedAt = now; }
    if (raw != b.stable && now - b.changedAt >= BTN_DEBOUNCE_MS) {
      b.stable = raw;
      g_activity = now;
      if (raw) { b.pressedAt = now; b.longDone = false; io::play(io::SND_CLICK); }
      else if (!b.longDone && !g_bothDone) session::call(b.call);                 // short press: raise
    }
    if (b.stable && !b.longDone && now - b.pressedAt >= BTN_CANCEL_CALL_MS && !(g_b[0].stable && g_b[1].stable)) {
      b.longDone = true;
      session::cancelCall(b.call);                                                 // long press: cancel
    }
  }
  const bool both = g_b[0].stable && g_b[1].stable;
  if (!both) { if (!g_b[0].stable && !g_b[1].stable) g_bothDone = false; return; }
  for (Btn& b : g_b) b.longDone = true;                                            // a two-button hold is neither a call nor a cancel
  if (!g_bothDone && now - (g_b[0].pressedAt > g_b[1].pressedAt ? g_b[0].pressedAt : g_b[1].pressedAt) >= BTN_SETUP_MS) {
    g_bothDone = true;
    g_setup = true;
    io::play(io::SND_STARTUP);
  }
}

bool takeSetupRequest() { const bool r = g_setup; g_setup = false; return r; }
uint32_t lastActivityMs() { return g_activity; }
bool down(uint8_t type) { return type >= 1 && type <= 2 && g_b[type - 1].stable; }

}  // namespace buttons
