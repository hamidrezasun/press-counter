#include "gw_update.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Updater.h>
#include <WiFiClientSecureBearSSL.h>
#include <memory>
#include "gw_config.h"
#include "gw_link.h"
#include "gw_settings.h"
#include "gw_state.h"

namespace fwupd {
namespace {

const char* MARKER_PATH = "/stm32.pending";       // "size crc32": an STM32 flash was started and not yet verified
const uint32_t RETRY_DELAY_MS = 20000UL;

enum Job : uint8_t { J_NONE, J_CHECK, J_APPLY, J_STM_RETRY };

Info g_info;
TickFn g_tick = nullptr;
Job g_job = J_NONE;
bool g_auto = false;                              // the running job was started by the timer, not by a person
uint32_t g_phaseAt = 0, g_lastTick = 0, g_nextAuto = 0, g_retryAt = 0;
uint8_t g_retries = 0;
bool g_markerAtBoot = false;

void setPhase(uint8_t phase, uint8_t pct, const char* msg) {
  g_info.phase = phase;
  g_info.pct = pct;
  if (msg) {
    pc::copyStr(g_info.msg, msg, sizeof(g_info.msg));
    gws::log("update: %s", msg);
  }
  g_phaseAt = millis();
}

void pump() {                                      // keep the STM32 display and the watchdog happy during long jobs
  yield();
  if (millis() - g_lastTick >= 1000UL) {
    g_lastTick = millis();
    if (g_tick) g_tick();
  }
}

uint16_t versionOf(const char* s) {                // "3.1" -> 0x0301
  uint16_t major = 0, minor = 0;
  while (*s >= '0' && *s <= '9') major = (uint16_t)(major * 10 + (*s++ - '0'));
  if (*s == '.') s++;
  while (*s >= '0' && *s <= '9') minor = (uint16_t)(minor * 10 + (*s++ - '0'));
  return (uint16_t)((major << 8) | (minor & 0xFF));
}

bool stmIdle() {
  const StmStatus& s = gwlink::status();
  return s.valid && s.relay == 0 && !s.inPress;
}

// ── tiny HTTP download helper ─────────────────────────────────────────────────────────────────────────────
class Download {
 public:
  ~Download() { _http.end(); }
  bool open(const String& url) {
    if (url.startsWith("https://")) {
      BearSSL::WiFiClientSecure* c = new BearSSL::WiFiClientSecure();
      c->setInsecure();                            // no certificate store on the chip: see the note in gw_update.h
      c->setBufferSizes(1024, 512);
      _client.reset(c);
    } else {
      _client.reset(new WiFiClient());
    }
    _http.setTimeout(8000);
    _http.setReuse(false);
    _http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!_http.begin(*_client, url)) return false;
    if (settings::cfg.updateToken[0]) _http.addHeader("Authorization", String("Bearer ") + settings::cfg.updateToken);
    _code = _http.GET();
    return _code == HTTP_CODE_OK;
  }
  int code() const { return _code; }
  int size() { return _http.getSize(); }           // -1 when the server sends no Content-Length
  String text() { return _http.getString(); }
  // >0 bytes read, 0 nothing yet, -1 the transfer ended
  int read(uint8_t* buf, size_t n) {
    WiFiClient* s = _http.getStreamPtr();
    if (!s) return -1;
    const int avail = s->available();
    if (avail > 0) return s->read(buf, n < (size_t)avail ? n : (size_t)avail);
    return s->connected() ? 0 : -1;
  }

 private:
  HTTPClient _http;
  std::unique_ptr<WiFiClient> _client;
  int _code = 0;
};

String absoluteUrl(const String& base, const char* path) {
  if (!strncmp(path, "http://", 7) || !strncmp(path, "https://", 8)) return String(path);
  String b = base;
  if (!b.endsWith("/")) b += "/";
  return b + (path[0] == '/' ? path + 1 : path);
}

String baseUrl() {
  String u = settings::cfg.updateUrl;
  if (u.endsWith(".json")) u.remove(u.lastIndexOf('/') + 1);
  return u;
}
String manifestUrl() {
  String u = settings::cfg.updateUrl;
  if (u.endsWith(".json")) return u;
  if (!u.endsWith("/")) u += "/";
  return u + "manifest.json";
}

// Reads a binary body into `sink`.  Returns the number of bytes, -1 on error / timeout.
typedef bool (*Sink)(const uint8_t* data, size_t n);
long fetchBinary(const String& url, long expected, Sink sink, uint8_t pctFrom, uint8_t pctTo, const char* what) {
  Download dl;
  if (!dl.open(url)) { gws::log("update: %s HTTP %d", what, dl.code()); return -1; }
  long total = dl.size();
  if (total <= 0) total = expected;
  if (total <= 0) { gws::log("update: %s: no Content-Length", what); return -1; }
  uint8_t buf[512];
  long got = 0;
  uint32_t lastData = millis();
  while (got < total) {
    pump();
    const int n = dl.read(buf, sizeof(buf));
    if (n < 0) break;
    if (n == 0) {
      if (millis() - lastData > 12000UL) { gws::log("update: %s timeout", what); return -1; }
      delay(2);
      continue;
    }
    lastData = millis();
    if (!sink(buf, (size_t)n)) return -1;
    got += n;
    g_info.pct = (uint8_t)(pctFrom + (uint32_t)(pctTo - pctFrom) * (uint32_t)got / (uint32_t)total);
  }
  return got == total ? got : -1;
}

// ── sinks ──────────────────────────────────────────────────────────────────────────────────────────────────────
File g_file;
uint32_t g_crc = 0;
long g_fileBytes = 0;

bool sinkFile(const uint8_t* d, size_t n) {
  if (g_fileBytes + (long)n > (long)STM32_MAX_IMAGE) return false;
  if (g_file.write(d, n) != n) return false;
  g_crc = pc::crc32Update(g_crc, d, n);
  g_fileBytes += (long)n;
  return true;
}
bool sinkEsp(const uint8_t* d, size_t n) { return Update.write(const_cast<uint8_t*>(d), n) == n; }

// ── STM32 ──────────────────────────────────────────────────────────────────────────────────────────────────────────
bool writeMarker(uint32_t size, uint32_t crc) {
  File f = LittleFS.open(MARKER_PATH, "w");
  if (!f) return false;
  f.printf("%lu %lu", (unsigned long)size, (unsigned long)crc);
  f.close();
  return true;
}

bool readMarker(uint32_t& size, uint32_t& crc) {
  File f = LittleFS.open(MARKER_PATH, "r");
  if (!f) return false;
  String s = f.readString();
  f.close();
  size = (uint32_t)s.toInt();
  const int sp = s.indexOf(' ');
  crc = sp > 0 ? (uint32_t)strtoul(s.c_str() + sp + 1, nullptr, 10) : 0;
  return size > 0;
}

// Flashes /stm32.bin (already checked against `crc`) and verifies it by reading it back.
bool flashStm(uint32_t size, uint32_t crc) {
  File f = LittleFS.open(STM32_IMAGE_PATH, "r");
  if (!f || f.size() != size) { setPhase(P_FAILED, 0, "stored STM32 image missing"); return false; }
  writeMarker(size, crc);
  setPhase(P_FLASH, 0, "STM32: entering bootloader");
  if (g_tick) g_tick();                            // last N frame: "flashing" on the display
  if (!gwlink::enterBootloader()) {
    f.close();
    setPhase(P_FAILED, 0, "STM32 bootloader does not answer (check UART / BOOT0 / NRST wires)");
    return false;
  }
  bool ok = gwlink::stmErase(STM32_FLASH_BASE, size);
  if (!ok) setPhase(P_FAILED, 0, "STM32: erase failed");
  uint8_t buf[256];
  for (uint32_t off = 0; ok && off < size; off += sizeof(buf)) {
    const size_t n = f.read(buf, sizeof(buf));
    size_t padded = n;
    while (padded & 3) buf[padded++] = 0xFF;      // the ROM bootloader writes whole words
    if (n == 0 || !gwlink::stmWrite(STM32_FLASH_BASE + off, buf, padded)) {
      setPhase(P_FAILED, 0, "STM32: write failed");
      ok = false;
      break;
    }
    g_info.pct = (uint8_t)(90UL * (off + n) / size);
    pump();
  }
  f.close();
  if (ok) {                                        // read back and compare the CRC32
    uint32_t c = 0;
    for (uint32_t off = 0; off < size; off += sizeof(buf)) {
      const size_t n = (size - off) < sizeof(buf) ? (size - off) : sizeof(buf);
      if (!gwlink::stmRead(STM32_FLASH_BASE + off, buf, n)) { ok = false; break; }
      c = pc::crc32Update(c, buf, n);
      g_info.pct = (uint8_t)(90UL + 10UL * (off + n) / size);
      pump();
    }
    if (ok && c != crc) ok = false;
    if (!ok) setPhase(P_FAILED, 0, "STM32: verification failed");
  }
  gwlink::leaveBootloader();                       // BOOT0 low + reset: the new program starts
  if (ok) {
    LittleFS.remove(MARKER_PATH);
    g_retries = 0;
    setPhase(P_DONE, 100, "STM32 updated");
  }
  return ok;
}

bool installStm(const String& url, long expected, uint32_t crc) {
  FSInfo fi;
  LittleFS.info(fi);
  if (fi.totalBytes - fi.usedBytes < (size_t)STM32_MAX_IMAGE + 8192) { setPhase(P_FAILED, 0, "no room for the STM32 image"); return false; }
  LittleFS.remove(STM32_IMAGE_PATH);
  LittleFS.remove(MARKER_PATH);
  g_file = LittleFS.open(STM32_IMAGE_PATH, "w");
  if (!g_file) { setPhase(P_FAILED, 0, "cannot create /stm32.bin"); return false; }
  g_crc = 0;
  g_fileBytes = 0;
  setPhase(P_DOWNLOAD, 0, "downloading STM32 image");
  const long n = fetchBinary(url, expected, sinkFile, 0, 100, "stm32");
  g_file.close();
  if (n <= 0) { LittleFS.remove(STM32_IMAGE_PATH); setPhase(P_FAILED, 0, "STM32 download failed"); return false; }
  if (crc && g_crc != crc) { LittleFS.remove(STM32_IMAGE_PATH); setPhase(P_FAILED, 0, "STM32 image CRC32 differs from the manifest"); return false; }
  return flashStm((uint32_t)n, g_crc);
}

// ── ESP8266 ────────────────────────────────────────────────────────────────────────────────────────────────────────
bool installEsp(const String& url, long expected, const char* md5) {
  setPhase(P_DOWNLOAD, 0, "downloading gateway image");
  Download probe;
  if (!probe.open(url)) { setPhase(P_FAILED, 0, "gateway image: HTTP error"); return false; }
  long total = probe.size();
  if (total <= 0) total = expected;
  if (total <= 0 || !Update.begin((size_t)total)) { setPhase(P_FAILED, 0, "gateway image: no room / unknown size"); return false; }
  if (md5 && strlen(md5) == 32) Update.setMD5(md5);
  uint8_t buf[512];
  long got = 0;
  uint32_t lastData = millis();
  while (got < total) {
    pump();
    const int n = probe.read(buf, sizeof(buf));
    if (n < 0) break;
    if (n == 0) {
      if (millis() - lastData > 12000UL) break;
      delay(2);
      continue;
    }
    lastData = millis();
    if (!sinkEsp(buf, (size_t)n)) break;
    got += n;
    g_info.pct = (uint8_t)(100UL * (uint32_t)got / (uint32_t)total);
  }
  if (got != total || !Update.end()) {
    Update.end();
    setPhase(P_FAILED, 0, "gateway image rejected (size / MD5)");
    return false;
  }
  setPhase(P_DONE, 100, "gateway updated - restarting");
  if (g_tick) g_tick();
  delay(800);
  ESP.restart();
  return true;
}

// ── manifest check ─────────────────────────────────────────────────────────────────────────────────────────────────
void runJob(bool apply) {
  if (WiFi.status() != WL_CONNECTED) { setPhase(P_FAILED, 0, "no WiFi"); return; }
  if (!settings::cfg.updateUrl[0]) { setPhase(P_FAILED, 0, "no update server configured"); return; }
  setPhase(P_CHECK, 0, "checking the update server");
  g_lastTick = 0;
  pump();
  StaticJsonDocument<768> doc;
  {
    Download dl;
    if (!dl.open(manifestUrl())) {
      char m[48];
      snprintf(m, sizeof(m), "manifest: HTTP %d", dl.code());
      g_info.manifestOk = false;
      setPhase(P_FAILED, 0, m);
      return;
    }
    if (dl.size() > 2048) { setPhase(P_FAILED, 0, "manifest too large"); return; }
    const DeserializationError e = deserializeJson(doc, dl.text());
    if (e) { g_info.manifestOk = false; setPhase(P_FAILED, 0, "manifest is not valid JSON"); return; }
  }
  g_info.manifestOk = true;
  g_info.lastCheckS = millis() / 1000UL + 1;
  JsonObject esp = doc["esp"], stm = doc["stm32"];
  pc::copyStr(g_info.espLatest, esp["version"] | "", sizeof(g_info.espLatest));
  pc::copyStr(g_info.stmLatest, stm["version"] | "", sizeof(g_info.stmLatest));
  g_info.espNew = g_info.espLatest[0] && versionOf(g_info.espLatest) > versionOf(GW_VERSION);
  const StmIdentity& id = gwlink::identity();
  const bool stmKnown = id.valid && gwlink::alive();
  g_info.stmNew = g_info.stmLatest[0] && (stmKnown ? versionOf(g_info.stmLatest) > versionOf(id.ver) : !g_auto);

  if (!apply || (!g_info.espNew && !g_info.stmNew)) {
    char m[60];
    if (g_info.espNew || g_info.stmNew) snprintf(m, sizeof(m), "update available: gateway %s, counter %s", g_info.espNew ? g_info.espLatest : "-", g_info.stmNew ? g_info.stmLatest : "-");
    else snprintf(m, sizeof(m), "up to date (gateway %s)", GW_VERSION);
    setPhase(P_IDLE, 0, m);
    return;
  }

  // the STM32 first (the ESP reboots at the end)
  if (g_info.stmNew) {
    if (stmKnown && !stmIdle()) {
      setPhase(P_FAILED, 0, "machine is running - STM32 update postponed");
    } else if (installStm(absoluteUrl(baseUrl(), stm["url"] | "counter.bin"), stm["size"] | 0, (uint32_t)strtoul(stm["crc32"] | "0", nullptr, 16))) {
      g_info.stmNew = false;
      delay(1500);
    } else {
      return;                                      // do not touch the gateway after a failed counter update
    }
  }
  if (g_info.espNew) installEsp(absoluteUrl(baseUrl(), esp["url"] | "gateway.bin"), esp["size"] | 0, esp["md5"] | "");
}

}  // namespace

void begin(TickFn tick) {
  g_tick = tick;
  memset(&g_info, 0, sizeof(g_info));
  g_nextAuto = millis() + 120000UL;                // not right after power-up: let WiFi / NTP / the STM32 settle
  uint32_t size, crc;
  if (readMarker(size, crc) && LittleFS.exists(STM32_IMAGE_PATH)) {
    g_markerAtBoot = true;
    g_retryAt = millis() + RETRY_DELAY_MS;
    gws::log("update: an STM32 flash was interrupted - will retry");
  } else if (LittleFS.exists(MARKER_PATH)) {
    LittleFS.remove(MARKER_PATH);
  }
}

void request(bool apply) {
  if (g_job != J_NONE) return;
  g_job = apply ? J_APPLY : J_CHECK;
  g_auto = false;
}

void requestStmRetry() {
  if (g_job != J_NONE) return;
  g_job = J_STM_RETRY;
  g_auto = false;
}

const Info& info() { return g_info; }
bool busy() { return g_job != J_NONE; }

void loop() {
  const uint32_t now = millis();
  if (g_job == J_NONE) {
    if ((g_info.phase == P_DONE || g_info.phase == P_FAILED) && now - g_phaseAt > 30000UL) g_info.phase = P_IDLE;
    if (g_markerAtBoot && (int32_t)(now - g_retryAt) >= 0) {
      if (g_retries < 3 && (!gwlink::alive() || stmIdle())) { g_retries++; g_job = J_STM_RETRY; g_auto = true; }
      else if (g_retries >= 3) g_markerAtBoot = false;
      g_retryAt = now + 60000UL;
    }
    if (settings::cfg.autoUpdate && settings::cfg.updateUrl[0] && WiFi.status() == WL_CONNECTED && (int32_t)(now - g_nextAuto) >= 0) {
      g_nextAuto = now + (uint32_t)settings::cfg.checkMinutes * 60000UL;
      g_job = J_APPLY;
      g_auto = true;
    }
    if (g_job == J_NONE) return;
  }
  const Job job = g_job;
  if (job == J_STM_RETRY) {
    uint32_t size = 0, crc = 0;
    if (readMarker(size, crc)) {
      gws::log("update: flashing the stored STM32 image (try %u)", g_retries);
      if (flashStm(size, crc)) g_markerAtBoot = false;
    } else {
      g_markerAtBoot = false;
    }
  } else {
    runJob(job == J_APPLY);
  }
  g_job = J_NONE;
  g_auto = false;
}

}  // namespace fwupd
