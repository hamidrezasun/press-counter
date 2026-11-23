// pc_sense.h - the two DC voltage inputs on ADC1, both divider 100k / 10k (0-30 V): 12/24 V sense (IO36, J7) and AUX (IO35, J9).
// The ESP32's eFuse ADC calibration is used (analogReadMilliVolts); the settings v12cal / auxcal trim the scale (permille).
#ifndef PC_SENSE_H
#define PC_SENSE_H

#include "pc_config.h"

namespace sense {
void begin();
void poll();                 // call from loop()
uint16_t v12mV();            // 12/24 V sense input, millivolt
uint16_t auxmV();            // AUX input, millivolt
}  // namespace sense

#endif
