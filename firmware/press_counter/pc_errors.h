// pc_errors.h - error / warning manager (codes in pc_proto.h).  Every raise / clear becomes an event (MQTT) and is shown on the display.
#ifndef PC_ERRORS_H
#define PC_ERRORS_H

#include "pc_config.h"

namespace err {
void begin();
void poll();                                              // expires time-limited errors
// Condition-driven errors (monitors call this every pass): raises when on, clears when off, no-op if unchanged.
void set(uint8_t code, bool on, uint16_t detail = 0);
// One-shot errors that stay until acknowledged (web / MQTT / console) or until ttlMs elapsed (0 = until acknowledged).
void latch(uint8_t code, uint16_t detail = 0, uint32_t ttlMs = 0);
void clear(uint8_t code);
void acknowledge();                                       // clears every latched error
bool active(uint8_t code);
uint16_t mask();                                          // bit n = error code n active
uint8_t top();                                            // most severe active code, 0 = none
uint8_t count();
uint16_t detail(uint8_t code);
}  // namespace err

#endif
