// pc_outputs.h - the four output relays OUT1..OUT4 (K2..K5, COM / NO / NC on J10..J13).
// Each output has a mode (settings out1..out4, pc::OutMode): manual (web page / MQTT relay/2..5 / console) or it follows the machine relay,
// any alert, the supervisor call, the net call or the press sense input.  All outputs are OFF after a reset.
#ifndef PC_OUTPUTS_H
#define PC_OUTPUTS_H

#include "pc_config.h"

namespace outputs {
void begin();
void poll();                                  // apply the modes (call every loop)
bool state(uint8_t index);                    // index 1..4
uint8_t mask();                               // bit n-1 = OUTn is on
// manual command for OUTn (index 1..4): state 0 = off (always allowed), 1 = on (only with 'remote start' enabled), 2 = toggle.
// Returns false if it was refused (output not in manual mode, remote start off).
bool command(uint8_t index, uint8_t state, uint8_t reason);
}  // namespace outputs

#endif
