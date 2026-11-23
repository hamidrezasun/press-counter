#include <driver/gpio.h>
#include "pc_io.h"
#include "pc_store.h"

namespace {
// on/off durations in ms, first entry = ON
const uint16_t PAT_CLICK[]   = {12};
const uint16_t PAT_SUCCESS[] = {80};
const uint16_t PAT_STARTUP[] = {60, 60, 60, 60, 160};
const uint16_t PAT_CALL[]    = {100, 100, 100, 100, 100};
const uint16_t PAT_WARNING[] = {120, 120, 120};
const uint16_t PAT_ERROR[]   = {300, 150, 300, 150, 300};

struct Pattern { const uint16_t* p; uint8_t n; };
const Pattern PATTERNS[] = {
  {PAT_CLICK, 1}, {PAT_SUCCESS, 1}, {PAT_STARTUP, 5}, {PAT_CALL, 5}, {PAT_WARNING, 3}, {PAT_ERROR, 5}};

const uint16_t* g_pat = nullptr;
uint8_t  g_patLen = 0, g_patPos = 0;
io::Sound g_patSound = io::SND_CLICK;
uint32_t g_patAt = 0;
bool     g_relay = false;
bool     g_red = false;
io::Alert g_alert = io::ALERT_OFF;

void buzz(bool on) { digitalWrite(PIN_BUZZER, on ? HIGH : LOW); }
}  // namespace

namespace io {

void outputLow(uint8_t pin) {
  // Output latch LOW first, then switch the pin to output: it never drives HIGH, not even for a moment.  (Arduino's digitalWrite() is
  // ignored on a pin that pinMode() has not configured yet, so the latch is written through the IDF driver.)
  gpio_set_level((gpio_num_t)pin, 0);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void begin() {
  outputLow(PIN_RELAY);
  g_relay = false;
  outputLow(PIN_BUZZER);
  outputLow(PIN_LED_ALERT);
}

void setRelayPin(bool on) {
  g_relay = on;
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
}
bool relayPin() { return g_relay; }

void play(Sound s) {
  const uint8_t mode = store::cfg.sound;
  if (mode == 0) return;
  if (mode == 1 && s != SND_ERROR && s != SND_WARNING && s != SND_CALL) return;
  if (g_pat && s < g_patSound) return;                               // a more important sound is playing
  g_pat = PATTERNS[s].p;
  g_patLen = PATTERNS[s].n;
  g_patSound = s;
  g_patPos = 0;
  g_patAt = millis();
  buzz(true);
}

void stopSound() { g_pat = nullptr; buzz(false); }
void setAlert(Alert alert) { g_alert = alert; }
bool alertLit() { return g_red; }

void poll() {
  const uint32_t now = millis();
  if (g_pat && now - g_patAt >= g_pat[g_patPos]) {
    g_patPos++;
    g_patAt = now;
    if (g_patPos >= g_patLen) { g_pat = nullptr; buzz(false); }
    else buzz(g_patPos % 2 == 0);
  }
  bool red = false;
  switch (g_alert) {
    case ALERT_STEADY: red = true; break;
    case ALERT_SLOW:   red = (now / 500) % 2 == 0; break;
    case ALERT_FAST:   red = (now / 125) % 2 == 0; break;
    default: break;
  }
  g_red = red;
  digitalWrite(PIN_LED_ALERT, red ? HIGH : LOW);
}

}  // namespace io
