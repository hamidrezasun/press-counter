// pc_trace.h - a tiny trail of the last thing the firmware did, kept in RTC memory.
//
// A panic (or any reset) loses the RAM log, so after a crash there is nothing to read.  RTC_NOINIT
// memory survives a software reset and a panic (it is lost only on power-off), so a few marks written
// on the way through risky code tell us exactly where a crash happened - readable over HTTP afterwards
// because setup() prints the previous mark into the normal log.
#ifndef PC_TRACE_H
#define PC_TRACE_H

#include <stdint.h>

namespace trace {
void boot();                                  // call first in setup(): keeps the trail and logs the previous one
void mark(uint8_t step, uint32_t arg = 0);    // record "now at step N"
const char* stepName(uint8_t step);           // human readable step
}  // namespace trace

#endif
