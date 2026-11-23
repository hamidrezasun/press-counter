// outputs.h - the four auxiliary output relays OUT1..OUT4 (K2..K5 on the main board).
// Each output has a mode (settings out1..out4, pc::OutMode): manual (web page / console / MQTT) or it follows the machine relay, any alert,
// the supervisor call, the net call or the press sense input.  At most pc::OUT_MAX_ON outputs are on at once (power budget of the 5 V module).
// All outputs are OFF after a reset.
#ifndef OUTPUTS_H
#define OUTPUTS_H

#include "config.h"

namespace outputs {
void begin();
void poll();                                  // call every loop: apply the modes
bool state(uint8_t index);                    // index 1..4
uint8_t mask();                               // bit n-1 = OUTn is on
// manual command for OUTn (index 1..4): state 0 = off (always allowed), 1 = on (only with 'remote start' enabled), 2 = toggle.
// Returns false if it was refused (output not in manual mode, remote start off, power limit).
bool command(uint8_t index, uint8_t state, uint8_t reason);
}  // namespace outputs

#endif
