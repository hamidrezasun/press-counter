#include "buttons.h"

namespace {
const uint8_t PINS[buttons::COUNT] = {PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_LEFT, PIN_BTN_RIGHT, PIN_BTN_OK, PIN_BTN_SUP, PIN_BTN_NET};
const char* const NAMES[buttons::COUNT] = {"UP", "DOWN", "LEFT", "RIGHT", "OK", "SUP", "NET"};

struct State {
  bool stable = false;                 // debounced: true = pressed
  bool raw = false;
  uint32_t changedAt = 0;
  uint32_t pressedAt = 0;
  bool longSent = false;
  uint32_t lastRepeat = 0;
};
State g_s[buttons::COUNT];
const uint8_t QN = 12;
buttons::Event g_q[QN];
uint8_t g_qh = 0, g_qt = 0;
uint32_t g_activity = 0;

void push(buttons::Btn b, buttons::Kind k) {
  const uint8_t nh = (uint8_t)((g_qh + 1) % QN);
  if (nh == g_qt) return;              // full: drop
  g_q[g_qh] = {b, k};
  g_qh = nh;
}
}  // namespace

namespace buttons {

void begin() {
  for (uint8_t i = 0; i < COUNT; i++) pinMode(PINS[i], INPUT_PULLUP);
  g_activity = millis();
}

void poll() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < COUNT; i++) {
    State& s = g_s[i];
    const bool raw = digitalRead(PINS[i]) == LOW;
    if (raw != s.raw) { s.raw = raw; s.changedAt = now; }
    if (raw != s.stable && now - s.changedAt >= BTN_DEBOUNCE_MS) {
      s.stable = raw;
      g_activity = now;
      if (raw) {
        s.pressedAt = now;
        s.longSent = false;
        s.lastRepeat = now;
        push((Btn)i, PRESS);
      } else if (!s.longSent) {
        push((Btn)i, SHORT);
      }
    }
    if (s.stable) {
      const uint32_t held = now - s.pressedAt;
      if (!s.longSent && held >= BTN_LONG_MS) {
        s.longSent = true;
        s.lastRepeat = now;
        push((Btn)i, LONG);
      } else if (s.longSent && i <= RIGHT && now - s.lastRepeat >= BTN_REPEAT_MS) {
        s.lastRepeat = now;
        push((Btn)i, REPEAT);
      }
      g_activity = now;                // a held button keeps the display awake
    }
  }
}

bool pop(Event& e) {
  if (g_qt == g_qh) return false;
  e = g_q[g_qt];
  g_qt = (uint8_t)((g_qt + 1) % QN);
  return true;
}

bool down(Btn b) { return g_s[b].stable; }
uint32_t heldMs(Btn b) { return g_s[b].stable ? millis() - g_s[b].pressedAt : 0; }
uint32_t lastActivityMs() { return g_activity; }
const char* name(Btn b) { return NAMES[b]; }

}  // namespace buttons
