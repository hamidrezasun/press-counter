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

namespace fwupd {
namespace {
Info g_info;
bool g_apply = false, g_auto = false;
volatile bool g_pending = false, g_running = false;
SemaphoreHandle_t g_sem = nullptr;
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

bool install(const char* url, long expected, const char* md5) {
  // An update must be possible in every situation: if the machine is enabled, switch it off first
  // (the relay drops the press contactor = safe state), then flash.  An active stroke signal is
  // tolerated - the running stroke is lost, the lifetime counter is flushed before the restart.
  if (session::relayOn()) {
    session::setRelay(false, pc::RR_REMOTE);
    plog::log("update: machine switched off for the update");
  }
  if (press::active()) plog::log("update: stroke signal active - flashing anyway");
  setPhase(P_DOWNLOAD, 0, "downloading firmware");
  HTTPClient http;
  std::unique_ptr<NetworkClient> client;
  if (!open(http, client, absoluteUrl(url))) { setPhase(P_FAILED, 0, "firmware image: HTTP error"); return false; }
  long total = http.getSize();
  if (total <= 0) total = expected;
  if (total <= 0 || !Update.begin((size_t)total)) { setPhase(P_FAILED, 0, "firmware image: no room / unknown size"); return false; }
  if (md5 && strlen(md5) == 32) Update.setMD5(md5);
  NetworkClient* s = http.getStreamPtr();
  uint8_t buf[1024];
  long got = 0;
  uint32_t lastData = millis();
  while (got < total) {
    // no esp_task_wdt_reset() here: this task is not subscribed to the watchdog.  The control loop
    // on core 1 keeps resetting it, and the download no longer blocks that task.
    const int avail = s ? s->available() : -1;
    if (avail < 0) break;
    if (avail == 0) {
      if (!s->connected() || millis() - lastData > 12000UL) break;
      delay(2);
      continue;
    }
    const int n = s->read(buf, avail < (int)sizeof(buf) ? avail : (int)sizeof(buf));
    if (n <= 0) continue;
    lastData = millis();
    if (Update.write(buf, (size_t)n) != (size_t)n) break;
    got += n;
    g_info.pct = (uint8_t)(100LL * got / total);
  }
  if (got != total || !Update.end()) {
    Update.abort();
    setPhase(P_FAILED, 0, "firmware image rejected (size / MD5)");
    err::latch(pc::ERR_UPDATE, 1, 600000UL);
    return false;
  }
  setPhase(P_DONE, 100, "updated - restarting");
  store::lifetimeFlush(true);
  delay(500);
  ESP.restart();
  return true;
}

void runJob(bool apply) {
  if (!net::wifiUp()) { setPhase(P_FAILED, 0, "no WiFi"); return; }
  if (!store::net.updateUrl[0]) { setPhase(P_FAILED, 0, "no update server configured"); return; }
  setPhase(P_CHECK, 0, "checking the update server");
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
  if (!g_info.isNew) { setPhase(P_IDLE, 0, "firmware is up to date"); return; }
  if (!apply) { setPhase(P_IDLE, 0, "new firmware available"); return; }
  install(e["url"] | "", e["size"] | 0L, e["md5"] | "");
}

// The download + flash run on core 0, so the control loop on core 1 keeps counting strokes and
// servicing the watchdog while a 1.3 MB image is pulled over a slow WiFi link.
void updateTask(void*) {
  for (;;) {
    if (xSemaphoreTake(g_sem, portMAX_DELAY) == pdTRUE) {
      const bool apply = g_apply;
      g_pending = false;
      g_running = true;
      runJob(apply);
      g_running = false;
    }
  }
}
}  // namespace

void begin() {
  memset(&g_info, 0, sizeof(g_info));
  g_nextAuto = millis() + 120000UL;
  if (!g_sem) g_sem = xSemaphoreCreateBinary();
  if (g_sem) xTaskCreatePinnedToCore(updateTask, "fwupd", 12288, nullptr, 1, nullptr, 0);
}

void request(bool apply) {
  if (g_pending || g_running) return;
  g_apply = apply;
  g_pending = true;
  if (g_sem) xSemaphoreGive(g_sem);
}

void loop() {
  if (g_pending || g_running) return;
  if (store::net.autoUpdate && store::net.updateUrl[0] && (int32_t)(millis() - g_nextAuto) >= 0) {
    g_nextAuto = millis() + (uint32_t)store::net.checkMinutes * 60000UL;
    if (!session::relayOn()) request(true);                  // automatic installs only while the machine is off
  }
}

const Info& info() { return g_info; }
bool busy() { return g_pending || g_running || (g_info.phase >= P_CHECK && g_info.phase <= P_FLASH); }

}  // namespace fwupd
