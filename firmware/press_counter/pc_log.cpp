#include "pc_log.h"
#include <stdarg.h>
#include <sys/time.h>

namespace {
char g_log[LOG_LINES][LOG_LINE_LEN];
int g_head = 0, g_count = 0;
const time_t PLAUSIBLE_AFTER = 1700000000;       // 2023-11-14
}  // namespace

namespace plog {

void log(const char* fmt, ...) {
  char text[LOG_LINE_LEN - 12];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);
  snprintf(g_log[g_head], LOG_LINE_LEN, "%lu %s", (unsigned long)(millis() / 1000UL), text);
#ifndef PC_NO_CONSOLE
  Serial.println(g_log[g_head]);
#endif
  g_head = (g_head + 1) % LOG_LINES;
  if (g_count < LOG_LINES) g_count++;
}

int count() { return g_count; }
const char* line(int i) {
  if (i < 0 || i >= g_count) return "";
  return g_log[(g_head - g_count + i + LOG_LINES * 2) % LOG_LINES];
}

}  // namespace plog

namespace clk {

bool valid() { return time(nullptr) > PLAUSIBLE_AFTER; }
uint32_t sec() { const time_t t = time(nullptr); return t > PLAUSIBLE_AFTER ? (uint32_t)t : 0; }
uint64_t ms() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec > PLAUSIBLE_AFTER ? (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000) : 0;
}

}  // namespace clk
