#include "gw_state.h"
#include "gw_link.h"
#include "gw_settings.h"
#include <stdarg.h>

namespace gws {
char lastTag[48] = "";
bool wifiUp = false, ntpOk = false, apMode = false;

namespace {
char g_log[LOG_LINES][LOG_LINE_LEN];
int g_head = 0, g_count = 0;
}

const char* deviceName() {
  return (gwlink::identity().valid && gwlink::identity().name[0]) ? gwlink::identity().name : settings::cfg.device;
}

void log(const char* fmt, ...) {
  char line[LOG_LINE_LEN - 12];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  snprintf(g_log[g_head], LOG_LINE_LEN, "%lu %s", (unsigned long)(millis() / 1000UL), line);
  g_head = (g_head + 1) % LOG_LINES;
  if (g_count < LOG_LINES) g_count++;
}

int logCount() { return g_count; }
const char* logLine(int i) {
  if (i < 0 || i >= g_count) return "";
  return g_log[(g_head - g_count + i + LOG_LINES * 2) % LOG_LINES];
}

}  // namespace gws
