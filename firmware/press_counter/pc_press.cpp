#include "pc_press.h"
#include <driver/gpio.h>
#include "pc_store.h"

namespace {
struct Edge { uint32_t us; uint8_t level; };
const uint8_t RING = 128;                      // power of two (a 3 s network stall with heavy contact bounce still fits)
volatile Edge g_ring[RING];
volatile uint8_t g_head = 0, g_tail = 0;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR isr() {
  portENTER_CRITICAL_ISR(&g_mux);
  const uint8_t h = g_head;
  g_ring[h].us = micros();
  g_ring[h].level = (uint8_t)gpio_get_level((gpio_num_t)PIN_PRESS);
  g_head = (uint8_t)((h + 1) & (RING - 1));
  if (g_head == g_tail) g_tail = (uint8_t)((g_tail + 1) & (RING - 1));        // overflow: drop the oldest edge
  portEXIT_CRITICAL_ISR(&g_mux);
}

enum State : uint8_t { IDLE, ACTIVE };
State    g_state = IDLE;
bool     g_gapOpen = false;                    // inactive edge seen inside an active period, waiting to see if it is bounce
bool     g_ignoreThis = false;                 // signal was already active at boot: that stroke is not counted
uint32_t g_startUs = 0, g_gapUs = 0;
uint32_t g_startMs = 0;
uint32_t g_count = 0, g_lastDur = 0, g_lastEndMs = 0, g_rejected = 0;
bool     g_ever = false;
bool     g_countEnabled = true;
press::StrokeCallback g_cb = nullptr;

void finish(uint32_t endUs) {
  const uint32_t durMs = (uint32_t)((endUs - g_startUs) / 1000UL);
  g_state = IDLE;
  g_gapOpen = false;
  if (g_ignoreThis) { g_ignoreThis = false; return; }
  if (!g_countEnabled) return;                       // die test: the stroke is timed but not counted
  if (durMs < store::cfg.minPulseMs) { g_rejected++; return; }
  g_count++;
  g_lastDur = durMs;
  g_lastEndMs = millis() ? millis() : 1;
  g_ever = true;
  store::lifetimeAdd(1);
  if (g_cb) g_cb(durMs);
}
}  // namespace

namespace press {

void begin() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  pinMode(PIN_PRESS, INPUT_PULLUP);            // bare devkit: nothing drives the pin, keep it high
#else
  pinMode(PIN_PRESS, INPUT);                   // 22k pull-up on the PCB (GPIO34 has no internal pull-up)
#endif
  g_head = 0;
  g_tail = 0;
  if (digitalRead(PIN_PRESS) == LOW) {         // already active at power-up
    g_state = ACTIVE;
    g_ignoreThis = true;
    g_startUs = micros();
    g_startMs = millis();
  }
  attachInterrupt(digitalPinToInterrupt(PIN_PRESS), isr, CHANGE);
}

void onStroke(StrokeCallback cb) { g_cb = cb; }

void setCountEnabled(bool on) {
  g_countEnabled = on;
  if (!on) { g_count = 0; g_lastDur = 0; g_lastEndMs = 0; g_ever = false; }   // the test run does not add up
}

void poll() {
  // drain the edge ring
  while (g_tail != g_head) {
    portENTER_CRITICAL(&g_mux);
    const Edge e = {g_ring[g_tail].us, g_ring[g_tail].level};
    g_tail = (uint8_t)((g_tail + 1) & (RING - 1));
    portEXIT_CRITICAL(&g_mux);
    if (e.level == LOW) {                      // press voltage appeared
      if (g_state == IDLE) { g_state = ACTIVE; g_startUs = e.us; g_startMs = millis(); g_gapOpen = false; }
      else g_gapOpen = false;                  // it came back inside the bounce window: same stroke
    } else {                                   // press voltage went away
      if (g_state == ACTIVE && !g_gapOpen) { g_gapOpen = true; g_gapUs = e.us; }
    }
  }
  // a gap that lasted longer than the bounce window ends the stroke
  if (g_state == ACTIVE && g_gapOpen && (uint32_t)(micros() - g_gapUs) >= PRESS_GAP_MS * 1000UL) finish(g_gapUs);
}

uint32_t count() { return g_count; }
void resetCount() { g_count = 0; g_lastDur = 0; g_lastEndMs = 0; g_ever = false; }
uint32_t lastDurationMs() { return g_lastDur; }
bool everCounted() { return g_ever; }
uint32_t sinceLastStrokeMs() { return g_ever ? millis() - g_lastEndMs : 0xFFFFFFFFUL; }
bool active() { return g_state == ACTIVE && !g_gapOpen; }
uint32_t activeForMs() { return g_state == ACTIVE ? millis() - g_startMs : 0; }
bool inTrain() { return g_ever && millis() - g_lastEndMs <= PRESS_TRAIN_GAP_MS; }
uint32_t rejected() { return g_rejected; }

}  // namespace press
