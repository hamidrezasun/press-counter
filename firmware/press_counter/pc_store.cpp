#include "pc_store.h"
#include <Preferences.h>
#include <stddef.h>

namespace {
Preferences g_prefs;

// ── machine settings parameter table ─────────────────────────────────────────────────────────────
enum Kind : uint8_t { K_U8, K_U16, K_I16, K_STR };
struct Param { const char* key; Kind kind; uint16_t off; int32_t lo, hi; };
#define PARAM(k, kind, field, lo, hi) { k, kind, (uint16_t)offsetof(Settings, field), lo, hi }
const Param PARAMS[] = {
  PARAM("name",     K_STR, name,         1,   NAME_LEN - 1),
  PARAM("minpulse", K_U16, minPulseMs,   1,   5000),
  PARAM("idle",     K_U16, idleMin,      0,   600),
  PARAM("stuck",    K_U16, stuckSec,     5,   3600),
  PARAM("tempoff",  K_I16, tempOffset10, -100, 100),
  PARAM("overtemp", K_I16, overTemp10,   300, 1200),
  PARAM("remote",   K_U8,  remoteStart,  0,   1),
  PARAM("sound",    K_U8,  sound,        0,   2),
  PARAM("bright",   K_U8,  brightness,   5,   100),
  PARAM("display",  K_U8,  displayOn,    0,   1),
  { "out1", K_U8, (uint16_t)(offsetof(Settings, outMode) + 0), 0, pc::OM_LAST },
  { "out2", K_U8, (uint16_t)(offsetof(Settings, outMode) + 1), 0, pc::OM_LAST },
  { "out3", K_U8, (uint16_t)(offsetof(Settings, outMode) + 2), 0, pc::OM_LAST },
  { "out4", K_U8, (uint16_t)(offsetof(Settings, outMode) + 3), 0, pc::OM_LAST },
  PARAM("v12cal",   K_U16, v12Cal,       500, 2000),
  PARAM("auxcal",   K_U16, auxCal,       500, 2000),
};
const int NPARAMS = sizeof(PARAMS) / sizeof(PARAMS[0]);
const uint16_t SETTINGS_VERSION = 8;        // machine settings layout
const uint16_t NET_VERSION = 6;             // network settings layout

uint32_t g_lifetime = 0, g_lifetimeSaved = 0;

void cp(char* dst, size_t n, const char* src) { pc::copyStr(dst, src ? src : "", n); }
}  // namespace

Settings store::cfg;
NetSettings store::net;

namespace store {

void defaults() {
  memset(&cfg, 0, sizeof(cfg));
  cp(cfg.name, sizeof(cfg.name), "press-1");
  cfg.minPulseMs = 10;
  cfg.idleMin = 10;
  cfg.stuckSec = 60;
  cfg.tempOffset10 = 0;
  cfg.overTemp10 = 700;
  cfg.remoteStart = 0;                 // starting a hydraulic press from the network is off unless the owner enables it
  cfg.sound = 2;
  cfg.brightness = 80;
  cfg.displayOn = 1;
  cfg.v12Cal = cfg.auxCal = 1000;
}

void netDefaults() {
  memset(&net, 0, sizeof(net));
  net.mqttPort = 1883;
  cp(net.mqttTopic, sizeof(net.mqttTopic), "press");
  net.autoUpdate = 0;
  net.checkMinutes = 360;
  cp(net.ntpHost, sizeof(net.ntpHost), "pool.ntp.org");
  cp(net.otaPass, sizeof(net.otaPass), "hydropress");
}

bool begin() {
  defaults();
  netDefaults();
  g_prefs.begin("pc", false);
  bool ok = false;
  // the structs are stored as blobs together with a version + size check (a firmware with another layout starts from defaults)
  if (g_prefs.getUShort("cfgver", 0) == SETTINGS_VERSION && g_prefs.getBytesLength("cfg") == sizeof(cfg)) {
    Settings tmp;
    if (g_prefs.getBytes("cfg", &tmp, sizeof(tmp)) == sizeof(tmp)) {
      tmp.name[NAME_LEN - 1] = '\0';
      cfg = tmp;
      ok = true;
    }
  }
  if (g_prefs.getUShort("netver", 0) == NET_VERSION && g_prefs.getBytesLength("net") == sizeof(net)) {
    NetSettings tmp;
    if (g_prefs.getBytes("net", &tmp, sizeof(tmp)) == sizeof(tmp)) net = tmp;
  }
  g_lifetime = g_lifetimeSaved = g_prefs.getULong("life", 0);
  return ok;
}

bool save() {
  g_prefs.putUShort("cfgver", SETTINGS_VERSION);
  return g_prefs.putBytes("cfg", &cfg, sizeof(cfg)) == sizeof(cfg);
}

bool saveNet() {
  g_prefs.putUShort("netver", NET_VERSION);
  return g_prefs.putBytes("net", &net, sizeof(net)) == sizeof(net);
}

bool setParam(const char* key, const char* value) {
  for (int i = 0; i < NPARAMS; i++) {
    const Param& p = PARAMS[i];
    if (strcmp(p.key, key) != 0) continue;
    uint8_t* base = (uint8_t*)&cfg + p.off;
    if (p.kind == K_STR) {
      char tmp[NAME_LEN];
      pc::sanitize(tmp, sizeof(tmp), value);
      for (char* c = tmp; *c; c++) if (*c == ' ' || *c == '/' || *c == '+' || *c == '#') *c = '-';   // names end up in MQTT topics
      const size_t n = strlen(tmp);
      if ((int32_t)n < p.lo || (int32_t)n > p.hi) return false;
      pc::copyStr((char*)base, tmp, NAME_LEN);
      return true;
    }
    int32_t v;
    if (!pc::parseInt(value, v) || v < p.lo || v > p.hi) return false;
    if (p.kind == K_U8) *base = (uint8_t)v;
    else if (p.kind == K_U16) { const uint16_t u = (uint16_t)v; memcpy(base, &u, 2); }
    else { const int16_t s = (int16_t)v; memcpy(base, &s, 2); }
    return true;
  }
  return false;
}

bool getParam(const char* key, char* out, size_t outSize) {
  for (int i = 0; i < NPARAMS; i++) {
    const Param& p = PARAMS[i];
    if (strcmp(p.key, key) != 0) continue;
    const uint8_t* base = (const uint8_t*)&cfg + p.off;
    if (p.kind == K_STR) { pc::copyStr(out, (const char*)base, outSize); return true; }
    int32_t v;
    if (p.kind == K_U8) v = *base;
    else if (p.kind == K_U16) { uint16_t u; memcpy(&u, base, 2); v = u; }
    else { int16_t s; memcpy(&s, base, 2); v = s; }
    snprintf(out, outSize, "%ld", (long)v);
    return true;
  }
  return false;
}

int paramCount() { return NPARAMS; }
const char* paramKey(int index) { return (index >= 0 && index < NPARAMS) ? PARAMS[index].key : ""; }

bool paramIsText(const char* key) {
  for (int i = 0; i < NPARAMS; i++) if (!strcmp(PARAMS[i].key, key)) return PARAMS[i].kind == K_STR;
  return true;
}

// One button step: booleans toggle, small ranges move by 1, wide ranges by 5 (clamped to the limits).
bool stepParam(const char* key, int32_t dir) {
  for (int i = 0; i < NPARAMS; i++) {
    const Param& p = PARAMS[i];
    if (strcmp(p.key, key) != 0) continue;
    if (p.kind == K_STR) return false;
    uint8_t* base = (uint8_t*)&cfg + p.off;
    int32_t v;
    if (p.kind == K_U8) v = *base;
    else if (p.kind == K_U16) { uint16_t u; memcpy(&u, base, 2); v = u; }
    else { int16_t s; memcpy(&s, base, 2); v = s; }
    const int32_t step = (p.hi - p.lo) <= 10 ? 1 : 5;
    if (p.hi == 1 && p.lo == 0 && dir) v = v ? 0 : 1;                 // yes/no setting
    else v += dir * step;
    if (v < p.lo) v = p.lo;
    if (v > p.hi) v = p.hi;
    if (p.kind == K_U8) *base = (uint8_t)v;
    else if (p.kind == K_U16) { const uint16_t u = (uint16_t)v; memcpy((uint8_t*)base, &u, 2); }
    else { const int16_t s = (int16_t)v; memcpy((uint8_t*)base, &s, 2); }
    return true;
  }
  return false;
}

uint32_t lifetime() { return g_lifetime; }
void lifetimeAdd(uint32_t n) { g_lifetime += n; }

void lifetimeFlush(bool force) {
  const uint32_t pending = g_lifetime - g_lifetimeSaved;
  if (pending == 0 || (!force && pending < LIFETIME_SAVE_EVERY)) return;
  if (g_prefs.putULong("life", g_lifetime) == sizeof(uint32_t)) g_lifetimeSaved = g_lifetime;
}

}  // namespace store
