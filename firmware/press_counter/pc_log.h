// pc_log.h - small shared helpers: log ring buffer (web page "Log" + console), wall clock (NTP).
#ifndef PC_LOG_H
#define PC_LOG_H

#include "pc_config.h"

namespace plog {
void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));   // also printed on the USB console
int count();
const char* line(int i);                       // 0 = oldest
}  // namespace plog

namespace clk {
bool valid();                                  // NTP time received (after 2023)
uint32_t sec();                                // unix seconds, 0 if not valid
uint64_t ms();                                 // unix milliseconds, 0 if not valid
}  // namespace clk

#endif
