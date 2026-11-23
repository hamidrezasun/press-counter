// clock.h - wall clock (Unix time) derived from millis(); synchronised by the gateway (NTP).
#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>

namespace clk {
void set(uint32_t sec, uint16_t ms);           // sync from NTP
bool valid();                                  // true once a plausible time (after 2023) was received
void now(uint32_t& sec, uint16_t& ms);         // 0 / 0 while not valid
uint32_t sec();                                // Unix seconds, 0 if not valid
uint32_t ageMs();                              // ms since the last sync (UINT32_MAX if never)
}  // namespace clk

#endif
