#include "pc_trace.h"
#include "pc_log.h"
#include <esp_attr.h>

namespace {
const uint32_t MAGIC = 0x50435452UL;         // "PCTR"
RTC_NOINIT_ATTR uint32_t s_magic;
RTC_NOINIT_ATTR uint32_t s_step;
RTC_NOINIT_ATTR uint32_t s_arg;
RTC_NOINIT_ATTR uint32_t s_runs;
RTC_NOINIT_ATTR uint32_t s_crashes;
}  // namespace

namespace trace {

const char* stepName(uint8_t step) {
  switch (step) {
    case 0:  return "idle (nothing risky)";
    case 1:  return "update requested";
    case 2:  return "fetching manifest.json";
    case 3:  return "opening the firmware URL";
    case 4:  return "Update.begin()";
    case 5:  return "downloading + flashing (arg = percent)";
    case 6:  return "Update.end()";
    case 7:  return "restarting into the new firmware";
    case 8:  return "web server paused for the update";
    case 9:  return "web server resumed";
    case 10: return "lifetime counter flush";
    case 11: return "image URL parsed";
    case 12: return "TCP connect failed (or crashed in connect)";
    case 13: return "TCP connected, sending the GET";
    case 14: return "request sent, reading the status line";
    case 15: return "server did not answer 200";
    case 16: return "headers read, Update.begin() next";
    case 17: return "Update.begin() failed";
    case 18: return "flashing the image";
    default: return "?";
  }
}

void boot() {
  if (s_magic != MAGIC) {                    // first boot (or after a power cycle): start clean
    s_magic = MAGIC;
    s_step = s_arg = 0;
    s_runs = 0;
    s_crashes = 0;
  }
  s_runs++;
  if (s_step || s_arg) {                     // the previous run died somewhere interesting
    s_crashes++;
    plog::log("trace: previous run stopped at step %u - %s (arg %u), crashes %u",
              (unsigned)s_step, stepName((uint8_t)s_step), (unsigned)s_arg, (unsigned)s_crashes);
  }
  s_step = 0;
  s_arg = 0;
}

void mark(uint8_t step, uint32_t arg) {
  s_step = step;
  s_arg = arg;
}

}  // namespace trace
