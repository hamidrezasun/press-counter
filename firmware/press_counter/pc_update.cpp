#include "pc_update.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <memory>
#include "pc_errors.h"
#include "pc_log.h"
#include "pc_net.h"
#include "pc_press.h"
#include "pc_session.h"
#include "pc_store.h"
#include "pc_web.h"
#include "pc_trace.h"

namespace fwupd {
namespace {
Info g_info;
bool g_apply = false, g_auto = false;
volatile bool g_pending = false, g_running = false;
uint32_t g_nextAuto = 0;

void setPhase(uint8_t phase, uint8_t pct, const char* msg) {
  g_info.phase = phase;
  g_info.pct = pct;
  if (msg) { pc::copyStr(g_info.msg, msg, sizeof(g_info.msg)); plog::log("update: %s", msg); }
}

uint16_t versionOf(const char* s) {                // "6.1" -> 0x0601
  uint16_t major = 0, minor = 0;
  while (*s >= '0' && *s <= '9') major = (uint16_t)(major * 10 + (*s++ - '0'));
  if (*s == '.') s++;
  while (*s >= '0' && *s <= '9') minor = (uint16_t)(minor * 10 + (*s++ - '0'));
  return (uint16_t)((major << 8) | (minor & 0xFF));
}

String manifestUrl() {
  String u = store::net.updateUrl;
  if (u.endsWith(".json")) return u;
  if (!u.endsWith("/")) u += "/";
  return u + "manifest.json";
}
String absoluteUrl(const char* path) {
  if (!strncmp(path, "http://", 7) || !strncmp(path, "https://", 8)) return String(path);
  String b = store::net.updateUrl;
  if (b.endsWith(".json")) b.remove(b.lastIndexOf('/') + 1);
  if (!b.endsWith("/")) b += "/";
  return b + (path[0] == '/' ? path + 1 : path);
}

bool open(HTTPClient& http, std::unique_ptr<NetworkClient>& client, const String& url) {
  if (url.startsWith("https://")) {
    NetworkClientSecure* c = new NetworkClientSecure();
    c->setInsecure();                              // no certificate store on the board: see the note in pc_update.h
    client.reset(c);
  } else {
    client.reset(new NetworkClient());
  }
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(*client, url)) return false;
  if (store::net.updateToken[0]) http.addHeader("Authorization", String("Bearer ") + store::net.updateToken);
  return http.GET() == HTTP_CODE_OK;
}

// ── image download over a raw socket ─────────────────────────────────────────────────────────────
// HTTPClient was where the board panicked (trace step 3) on this image, both from a task and from the
// control loop, with an 8 kB and with a 20 kB stack.  The transfer is small and the server is a plain
// static file host, so it is done here with a socket we control, with a trace mark at every stage.
struct Url {
  bool tls = false;
  String host, path;
  uint16_t port = 80;
};

bool parseUrl(const String& url, Url& u) {
  const int p = url.indexOf("://");
  if (p < 0) return false;
  u.tls = url.startsWith("https://");
  const String rest = url.substring(p + 3);
  const int slash = rest.indexOf('/');
  String hp = slash < 0 ? rest : rest.substring(0, slash);
  u.path = slash < 0 ? "/" : rest.substring(slash);
  const int colon = hp.indexOf(':');
  if (colon >= 0) {
    u.port = (uint16_t)hp.substring(colon + 1).toInt();
    hp = hp.substring(0, colon);
  } else {
    u.port = u.tls ? 443 : 80;
  }
  u.host = hp;
  return u.host.length() > 0 && u.port > 0;
}

bool downloadImage(const char* url, long expected, const char* md5) {
  Url u;
  if (!parseUrl(absoluteUrl(url), u)) { setPhase(P_FAILED, 0, "firmware URL is not usable"); return false; }
  trace::mark(11);
  NetworkClient c;
  c.setTimeout(8);
  if (!c.connect(u.host.c_str(), u.port)) {
    trace::mark(12);
    setPhase(P_FAILED, 0, "firmware image: cannot connect");
    return false;
  }
  trace::mark(13);
  c.printf("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: press-counter/%s\r\nConnection: close\r\n",
           u.path.c_str(), u.host.c_str(), PC_FW_VERSION_STR);
  if (store::net.updateToken[0]) c.printf("Authorization: Bearer %s\r\n", store::net.updateToken);
  c.print("\r\n");
  trace::mark(14);

  String line = c.readStringUntil('\n');                       // status line
  if (line.indexOf(" 200") < 0) { trace::mark(15); setPhase(P_FAILED, 0, "firmware image: HTTP error"); return false; }
  long clen = -1;
  for (int guard = 0; guard < 40; guard++) {                    // headers
    line = c.readStringUntil('\n');
    if (line.length() == 0 || line == "\r") break;
    if (line.startsWith("Content-Length:")) clen = line.substring(15).toInt();
  }
  trace::mark(16);

  long total = clen > 0 ? clen : expected;
  if (total <= 0 || !Update.begin((size_t)total)) { trace::mark(17); setPhase(P_FAILED, 0, "firmware image: no room / unknown size"); return false; }
  if (md5 && strlen(md5) == 32) Update.setMD5(md5);
  trace::mark(18);

  static uint8_t buf[1024];                                     // keep 1 kB off the task stack
  long got = 0;
  uint32_t lastData = millis();
  while (got < total) {
    esp_task_wdt_reset();                                       // this task owns the watchdog while it flashes
    const int avail = c.available();
    if (avail == 0) {
      if (!c.connected() || millis() - lastData > 12000UL) break;
      delay(2);
      continue;
    }
    const int n = c.read(buf, avail < (int)sizeof(buf) ? avail : (int)sizeof(buf));
    if (n <= 0) continue;
    lastData = millis();
    trace::mark(5, (uint32_t)(100LL * got / total));
    if (Update.write(buf, (size_t)n) != (size_t)n) break;
    esp_task_wdt_reset();
    got += n;
    g_info.pct = (uint8_t)(100LL * got / total);
  }
  c.stop();
  trace::mark(6);
  if (got != total || !Update.end()) {
    Update.abort();
    setPhase(P_FAILED, 0, "firmware image rejected (size / MD5)");
    err::latch(pc::ERR_UPDATE, 1, 600000UL);
    trace::mark(9);
    return false;
  }
  trace::mark(7);
  setPhase(P_DONE, 100, "updated - restarting");
  delay(500);
  ESP.restart();
  return false;
}

bool install(const char* url, long expected, const char* md5) {
  // The machine relay is dropped by request() in the caller's context (see the note there).
  if (press::active()) plog::log("update: stroke signal active - flashing anyway");
  setPhase(P_DOWNLOAD, 0, "downloading firmware");
  web::pause();                                   // nothing else may touch the network while this runs
  trace::mark(8);
  plog::log("update: stack headroom %u bytes before the firmware GET",
            (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
  store::lifetimeFlush(true);                     // persist the counter while the flash is still idle
  const bool ok = downloadImage(url, expected, md5);   // restarts the board when it succeeds
  web::resume();                                       // only reached on failure
  return ok;
}

void runJob(bool apply) {
  if (!net::wifiUp()) { setPhase(P_FAILED, 0, "no WiFi"); return; }
  if (!store::net.updateUrl[0]) { setPhase(P_FAILED, 0, "no update server configured"); return; }
  setPhase(P_CHECK, 0, "checking the update server");
  trace::mark(2);
  DynamicJsonDocument doc(768);
  {
    HTTPClient http;
    std::unique_ptr<NetworkClient> client;
    if (!open(http, client, manifestUrl())) { setPhase(P_FAILED, 0, "manifest.json not reachable"); return; }
    if (deserializeJson(doc, http.getString())) { setPhase(P_FAILED, 0, "manifest.json is not valid JSON"); return; }
  }
  JsonObject e = doc["esp32"];
  if (e.isNull()) { setPhase(P_FAILED, 0, "manifest has no \"esp32\" entry"); return; }
  pc::copyStr(g_info.latest, e["version"] | "", sizeof(g_info.latest));
  g_info.isNew = versionOf(g_info.latest) > versionOf(PC_FW_VERSION_STR);
  if (!g_info.isNew) { setPhase(P_IDLE, 0, "firmware is up to date"); trace::mark(0); return; }
  if (!apply) { setPhase(P_IDLE, 0, "new firmware available"); trace::mark(0); return; }
  install(e["url"] | "", e["size"] | 0L, e["md5"] | "");
}

// The download + flash run in the control loop (core 1), exactly like the Arduino HTTPUpdate
// examples: the task that writes flash is the task that owns the watchdog, and no other task on the
// same core can execute cached code while the flash cache is disabled mid-write.  Writing flash from
// a task pinned to core 0 (where the WiFi/lwIP tasks live) panicked the board during an update.
}  // namespace

void begin() {
  memset(&g_info, 0, sizeof(g_info));
  g_nextAuto = millis() + 120000UL;
}

void request(bool apply) {
  if (g_pending || g_running) return;
  // An update must be possible in every situation: drop the machine relay first (safe state).  This
  // runs in the caller's context, so the MQTT event it produces stays on the control loop while the
  // update task owns the network stack.
  if (session::relayOn()) {
    session::setRelay(false, pc::RR_REMOTE);
    plog::log("update: machine switched off for the update");
  }
  g_apply = apply;
  g_pending = true;
}

void loop() {
  if (g_pending) {                                       // run the job in this task (the control loop)
    g_pending = false;
    g_running = true;
    runJob(g_apply);
    g_running = false;
    return;
  }
  if (g_running) return;
  if (store::net.autoUpdate && store::net.updateUrl[0] && (int32_t)(millis() - g_nextAuto) >= 0) {
    g_nextAuto = millis() + (uint32_t)store::net.checkMinutes * 60000UL;
    // automatic installs only while the machine is off and no stroke is in progress
    if (!session::relayOn() && !press::active()) request(true);
  }
}

const Info& info() { return g_info; }
bool busy() { return g_pending || g_running || (g_info.phase >= P_CHECK && g_info.phase <= P_FLASH); }

}  // namespace fwupd
