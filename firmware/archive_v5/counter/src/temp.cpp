#include "temp.h"
#include "store.h"
#include "errors.h"

namespace {
const uint16_t VREFINT_MV = 1200;       // typical STM32F1 internal reference (no factory calibration value on the F1)
bool     g_valid = false;
int16_t  g_t10 = INT16_MIN;
int16_t  g_mcu10 = 0;
uint16_t g_vdda = 3300;
uint32_t g_last = 0;
bool     g_first = true;

uint32_t sampleAvg(uint32_t pin, int n) {
  uint32_t sum = 0;
  for (int i = 0; i < n; i++) sum += analogRead(pin);
  return sum / (uint32_t)n;
}
}  // namespace

namespace temp {

void begin() {
  analogReadResolution(12);
  pinMode(PIN_TEMP, INPUT_ANALOG);
  g_last = millis() - TEMP_SAMPLE_MS;
}

void poll() {
  const uint32_t now = millis();
  if (now - g_last < TEMP_SAMPLE_MS) return;
  g_last = now;

#ifdef AVREF
  const uint32_t vref = sampleAvg(AVREF, 8);
  if (vref > 100) g_vdda = (uint16_t)((uint32_t)VREFINT_MV * 4095UL / vref);
#endif
  const uint32_t raw = sampleAvg(PIN_TEMP, 16);
  // LM35: 10 mV per degC  ->  mV equals tenths of a degree
  const int32_t mv = (int32_t)(raw * (uint32_t)g_vdda / 4095UL);
  const bool ok = raw > 8 && raw < 3600;                // < ~7 mV: open / shorted to ground; > ~2.9 V: shorted to +5 V
  g_valid = ok;
  err::set(pc::ERR_TEMP, !ok);
  if (ok) {
    const int16_t t10 = (int16_t)(mv + store::cfg.tempOffset10);
    g_t10 = g_first ? t10 : (int16_t)((3 * g_t10 + t10 + 2) / 4);        // light smoothing
    g_first = false;
    err::set(pc::ERR_OVERTEMP, g_t10 > store::cfg.overTemp10);
  } else {
    g_t10 = INT16_MIN;
    g_first = true;
    err::set(pc::ERR_OVERTEMP, false);
  }
#ifdef ATEMP
  {
    const uint32_t r = sampleAvg(ATEMP, 8);
    const int32_t vs = (int32_t)(r * (uint32_t)g_vdda / 4095UL);          // mV
    g_mcu10 = (int16_t)(((1430 - vs) * 100) / 43 + 250);                   // 25 degC at 1.43 V, -4.3 mV/degC
  }
#endif
}

bool valid() { return g_valid; }
int16_t tempC10() { return g_valid ? g_t10 : INT16_MIN; }
int16_t mcuTempC10() { return g_mcu10; }
uint16_t vddaMilliVolt() { return g_vdda; }

}  // namespace temp
