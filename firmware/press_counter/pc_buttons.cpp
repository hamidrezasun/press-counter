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
bool g_menuReq = false, g_menuDone = false, g_menuMode = false;
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
      if (raw) { b.pressedAt = now; b.longDone = false; g_menuDone = false; io::play(io::SND_CLICK); }
      else if (!b.longDone && !g_menuMode) session::call(b.call);            // short press: raise the call
    }
    // 1.5 s: cancel the call of this button (in the menu the buttons only drive the menu)
    if (!g_menuMode && b.stable && !b.longDone && now - b.pressedAt >= BTN_CANCEL_CALL_MS) {
      b.longDone = true;
      session::cancelCall(b.call);
    }
    // CALL NET held 3 s opens the local menu.  No gesture ever needs both buttons at once.
    if (!g_menuMode && b.stable && b.call == pc::CALL_NET && !g_menuDone && now - b.pressedAt >= BTN_MENU_MS) {
      g_menuDone = true;
      g_menuReq = true;
      io::play(io::SND_STARTUP);
    }
  }
}

bool takeMenuRequest() { const bool r = g_menuReq; g_menuReq = false; return r; }

void setMenuMode(bool on) {
  g_menuMode = on;
  if (on) for (Btn& b : g_b) b.longDone = true;         // the press that opened the menu does nothing else
}

uint32_t lastActivityMs() { return g_activity; }
bool down(uint8_t type) { return type >= 1 && type <= 2 && g_b[type - 1].stable; }

}  // namespace buttons
