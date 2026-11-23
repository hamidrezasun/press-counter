#include "sense.h"
#include "store.h"
#include "temp.h"

namespace {
const uint16_t WINDOW = 400;                 // 400 samples at 2 kHz = 200 ms
const uint32_t MV_PER_MV_X1000 = 700;        // nominal: 0.7 V of mains per mV rms at the pin (about 220 V <-> 314 mV): calibrate with 'accal' / the module trimmer
const uint32_t V12_DIV_NUM = 43, V12_DIV_DEN = 10;   // (33k + 10k) / 10k

// ISR side
volatile uint8_t  g_chan = 3;                // channel of the conversion that is running
volatile uint16_t g_cnt = 0, g_tick = 0;
volatile uint32_t g_sum = 0;
volatile uint64_t g_sq = 0;
volatile uint32_t g_v12sum = 0;
volatile uint16_t g_v12n = 0;
volatile bool     g_ready = false;
volatile uint32_t g_wSum = 0;
volatile uint64_t g_wSq = 0;

uint16_t g_v12mV = 0, g_vac10 = 0;
bool g_valid = false;

uint32_t isqrt(uint32_t v) {
  uint32_t r = 0, bit = 1UL << 30;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
    else r >>= 1;
    bit >>= 2;
  }
  return r;
}
}  // namespace

extern "C" void ADC1_2_IRQHandler(void) {
  if (!(ADC2->SR & ADC_SR_EOC)) return;
  const uint32_t x = ADC2->DR & 0xFFF;                       // reading DR clears EOC
  if (g_chan == 2) {
    g_v12sum += x;
    g_v12n++;
    g_chan = 3;
    ADC2->SQR3 = 3;
    return;
  }
  g_sum += x;
  g_sq += (uint64_t)x * x;
  if (++g_cnt >= WINDOW) {
    g_wSum = g_sum; g_wSq = g_sq; g_ready = true;
    g_sum = 0; g_sq = 0; g_cnt = 0;
  }
  if ((++g_tick & 63) == 0) { g_chan = 2; ADC2->SQR3 = 2; }  // every 64th conversion measures the 12 V input
}

namespace sense {

void begin() {
  RCC->APB2ENR |= RCC_APB2ENR_ADC2EN | RCC_APB2ENR_IOPAEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV6;        // 72 MHz / 6 = 12 MHz (max. 14 MHz)
  GPIOA->CRL &= ~((0xFUL << 8) | (0xFUL << 12));                           // PA2, PA3: analog input
  ADC2->SMPR2 = (7UL << 6) | (7UL << 9);                                   // channels 2 and 3: 239.5 cycles (high source impedance of the divider)
  ADC2->SQR1 = 0;
  ADC2->SQR3 = 3;
  ADC2->CR1 = ADC_CR1_EOCIE;
  ADC2->CR2 = ADC_CR2_ADON;
  delayMicroseconds(20);
  ADC2->CR2 |= ADC_CR2_RSTCAL;
  while (ADC2->CR2 & ADC_CR2_RSTCAL) {}
  ADC2->CR2 |= ADC_CR2_CAL;
  while (ADC2->CR2 & ADC_CR2_CAL) {}
  ADC2->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_EXTSEL_2 | ADC_CR2_EXTSEL_0;   // external trigger: TIM4 CC4
  // TIM4: 2 kHz, channel 4 in PWM mode only produces the internal compare event that starts the conversion (the pin stays a GPIO)
  TIM4->PSC = 71;
  TIM4->ARR = 499;
  TIM4->CCR4 = 250;
  TIM4->CCMR2 = TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;
  TIM4->CCER = TIM_CCER_CC4E;
  TIM4->EGR = TIM_EGR_UG;
  NVIC_SetPriority(ADC1_2_IRQn, 3);
  NVIC_EnableIRQ(ADC1_2_IRQn);
  TIM4->CR1 = TIM_CR1_CEN;
}

void poll() {
  static uint32_t lastV12 = 0;
  if (g_ready) {
    noInterrupts();
    const uint32_t sum = g_wSum;
    const uint64_t sq = g_wSq;
    g_ready = false;
    interrupts();
    const uint32_t mean = sum / WINDOW;
    const uint64_t ms = sq / WINDOW;
    const uint32_t var = ms > (uint64_t)mean * mean ? (uint32_t)(ms - (uint64_t)mean * mean) : 0;
    const uint32_t counts = isqrt(var);
    const uint32_t vdda = temp::vddaMilliVolt() ? temp::vddaMilliVolt() : 3300;
    const uint32_t pin_mV = counts * vdda / 4096UL;
    uint32_t v10 = pin_mV < 8 ? 0 : (pin_mV * MV_PER_MV_X1000 / 100UL) * store::cfg.acCal / 1000UL;   // 0.1 V units
    g_vac10 = v10 > 65535UL ? 65535 : (uint16_t)v10;
    g_valid = true;
  }
  if (millis() - lastV12 >= 500 && g_v12n >= 8) {
    lastV12 = millis();
    noInterrupts();
    const uint32_t s = g_v12sum;
    const uint16_t n = g_v12n;
    g_v12sum = 0; g_v12n = 0;
    interrupts();
    const uint32_t vdda = temp::vddaMilliVolt() ? temp::vddaMilliVolt() : 3300;
    const uint32_t pin_mV = (s / n) * vdda / 4096UL;
    const uint32_t v = pin_mV * V12_DIV_NUM / V12_DIV_DEN * store::cfg.v12Cal / 1000UL;
    g_v12mV = v > 65535UL ? 65535 : (uint16_t)v;
  }
}

bool valid() { return g_valid; }
uint16_t v12mV() { return g_v12mV; }
uint16_t vac10() { return g_vac10; }

}  // namespace sense
