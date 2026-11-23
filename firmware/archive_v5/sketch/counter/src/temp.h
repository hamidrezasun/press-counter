// temp.h - LM35 temperature (10 mV/degC) on PB1 with the internal reference compensating the supply, plus the MCU die temperature.
#ifndef TEMP_H
#define TEMP_H

#include "config.h"

namespace temp {
void begin();
void poll();                        // call every loop (samples every TEMP_SAMPLE_MS)
bool valid();                       // false when the LM35 is open / shorted
int16_t tempC10();                  // 0.1 degC, calibration offset applied; INT16_MIN when invalid
int16_t mcuTempC10();               // STM32 internal sensor (diagnostics only, +-5 degC)
uint16_t vddaMilliVolt();           // measured supply voltage, mV (from the internal 1.2 V reference)
}  // namespace temp

#endif
