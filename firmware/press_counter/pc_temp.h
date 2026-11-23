// pc_temp.h - DS18B20 temperature probe on a 1-wire bus (GPIO4).  Own bit-banged driver, one sensor (skip ROM), 12 bit.
// A conversion takes 750 ms: it is started and read back later, the loop never waits for it.
#ifndef PC_TEMP_H
#define PC_TEMP_H

#include "pc_config.h"

namespace temp {
void begin();
void poll();
bool valid();                 // a good reading within the last 10 s
int16_t tempC10();            // 0.1 degC incl. the offset setting, INT16_MIN if invalid
uint32_t crcErrors();
}  // namespace temp

#endif
