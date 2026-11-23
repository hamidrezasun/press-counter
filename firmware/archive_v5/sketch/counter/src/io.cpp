#include "io.h"
#include "store.h"

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
bool     g_buzz = false;
bool     g_relay = false;
bool     g_runLed = false;
io::Alert g_alert = io::ALERT_OFF;

void buzz(bool on) { g_buzz = on; digitalWrite(PIN_BUZZER, on ? HIGH : LOW); }

// Back-light PWM: TIM3 channel 3 on PB0 (no remap), 72 MHz / 12 / 256 = 23 kHz.  Direct register access: analogWrite() would link the
// complete timer HAL (about 12 KB of flash that the 64 KB chip cannot spare).
void backlightInit() {
  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  GPIOB->CRL = (GPIOB->CRL & ~0xFUL) | 0xAUL;                          // PB0: alternate function push-pull, 2 MHz
  TIM3->PSC = 11;
  TIM3->ARR = 255;
  TIM3->CCR3 = 0;
  TIM3->CCMR2 = (TIM3->CCMR2 & ~0x00FFUL) | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;   // PWM mode 1
  TIM3->CCER |= TIM_CCER_CC3E;
  TIM3->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
  TIM3->EGR = TIM_EGR_UG;
}
}  // namespace

namespace io {

void begin() {
  digitalWrite(PIN_RELAY, RELAY_ON_LEVEL == HIGH ? LOW : HIGH);      // level first, then direction: no glitch
  pinMode(PIN_RELAY, OUTPUT);
  g_relay = false;
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_RUN, OUTPUT);
  pinMode(PIN_LED_ALERT, OUTPUT);
  digitalWrite(PIN_LED_RUN, LOW);
  digitalWrite(PIN_LED_ALERT, LOW);
  backlightInit();
  setBacklight(80);
  // PB11, PA2, PA3, PC13-PC15 are the output relays: outputs::begin() configures them
}

void setRelayPin(bool on) {
  g_relay = on;
  digitalWrite(PIN_RELAY, on ? RELAY_ON_LEVEL : (RELAY_ON_LEVEL == HIGH ? LOW : HIGH));
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
bool buzzerOn() { return g_buzz; }

void setLeds(bool run, Alert alert) { g_runLed = run; g_alert = alert; }

void setBacklight(uint8_t percent) {
  if (percent > 100) percent = 100;
  TIM3->CCR3 = (uint32_t)percent * 255UL / 100UL;
}

void poll() {
  const uint32_t now = millis();
  if (g_pat && now - g_patAt >= g_pat[g_patPos]) {
    g_patPos++;
    g_patAt = now;
    if (g_patPos >= g_patLen) { g_pat = nullptr; buzz(false); }
    else buzz(g_patPos % 2 == 0);
  }
  digitalWrite(PIN_LED_RUN, g_runLed ? HIGH : LOW);
  bool red = false;
  switch (g_alert) {
    case ALERT_STEADY: red = true; break;
    case ALERT_SLOW:   red = (now / 500) % 2 == 0; break;
    case ALERT_FAST:   red = (now / 125) % 2 == 0; break;
    default: break;
  }
  digitalWrite(PIN_LED_ALERT, red ? HIGH : LOW);
}

}  // namespace io
