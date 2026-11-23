// sense.h - voltage measurement on the two free ADC pins: PA2 = 12 V DC input, PA3 = 230 V AC (ZMPT101B module, mid-supply biased sine).
//
// ADC2 is driven by TIM4 at 2 kHz and an interrupt accumulates the samples (the core's analogRead() uses ADC1 for the LM35 and is not disturbed).
// 230 V: RMS over a 200 ms window (10 cycles at 50 Hz, 12 at 60 Hz) = sqrt(mean(x^2) - mean(x)^2), converted with the nominal module gain
// and the setting 'accal' (permille).  12 V: average, divider 33k / 10k (x4.3) and the setting 'v12cal'.
#ifndef SENSE_H
#define SENSE_H

#include "config.h"

namespace sense {
void begin();
void poll();                       // call every loop: turn the accumulated samples into volts
bool valid();                      // at least one complete measurement window
uint16_t v12mV();                  // 12 V input, millivolt
uint16_t vac10();                  // 230 V input, RMS, 0.1 V units (0 = no signal / module absent)
}  // namespace sense

#endif
