#include "pc_sense.h"
#include "pc_store.h"

namespace {
uint32_t g_at = 0;
uint32_t g_sum12 = 0, g_sumAux = 0;
uint8_t  g_n = 0;
uint32_t g_pin12 = 0, g_pinAux = 0;      // averaged pin voltages, mV
bool     g_valid = false;

uint16_t scale(uint32_t pinMv, float divider, uint16_t calPermille) {
  const float v = pinMv * divider * calPermille / 1000.0f;
  return (uint16_t)(v > 65000.0f ? 65000.0f : v);
}
}  // namespace

namespace sense {

void begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_V12, ADC_11db);       // 0 .. ~3.1 V at the pin = 0 .. ~34 V at the terminal
  analogSetPinAttenuation(PIN_AUX, ADC_11db);
}

// Both DC inputs are read every SENSE_SAMPLE_MS and averaged over SENSE_AVERAGE samples (also smooths 50 Hz ripple of rectified supplies).
void poll() {
  const uint32_t now = millis();
  if (now - g_at < SENSE_SAMPLE_MS) return;
  g_at = now;
  g_sum12 += analogReadMilliVolts(PIN_V12);
  g_sumAux += analogReadMilliVolts(PIN_AUX);
  if (++g_n < SENSE_AVERAGE) return;
  g_pin12 = g_sum12 / g_n;
  g_pinAux = g_sumAux / g_n;
  g_sum12 = g_sumAux = 0;
  g_n = 0;
  g_valid = true;
}

uint16_t v12mV() { return g_valid ? scale(g_pin12, V12_DIVIDER, store::cfg.v12Cal) : 0; }
uint16_t auxmV() { return g_valid ? scale(g_pinAux, AUX_DIVIDER, store::cfg.auxCal) : 0; }

}  // namespace sense
