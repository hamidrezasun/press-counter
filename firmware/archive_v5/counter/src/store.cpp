#include "store.h"
#include <stddef.h>

// ── minimal STM32F1 flash driver (RM0008 section 3).  Half-word programming, 1 KB pages. ──────────
namespace {
const uint32_t F_PG = 1u << 0, F_PER = 1u << 1, F_STRT = 1u << 6, F_LOCK = 1u << 7;
const uint32_t S_BSY = 1u << 0, S_PGERR = 1u << 2, S_WRPRT = 1u << 4, S_EOP = 1u << 5;
const uint32_t S_ERR = S_PGERR | S_WRPRT;

bool flashWait() {
  uint32_t guard = 2000000UL;
  while ((FLASH->SR & S_BSY) && --guard) {}
  return guard != 0;
}
void flashUnlock() {
  if (FLASH->CR & F_LOCK) {
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
  }
}
void flashLock() { FLASH->CR |= F_LOCK; }

bool erasePage(uint32_t addr) {
  if (!flashWait()) return false;
  FLASH->SR = S_ERR | S_EOP;
  FLASH->CR |= F_PER;
  FLASH->AR = addr;
  FLASH->CR |= F_STRT;
  const bool done = flashWait();
  FLASH->CR &= ~F_PER;
  const bool ok = done && !(FLASH->SR & S_ERR);
  FLASH->SR = S_ERR | S_EOP;
  return ok;
}

bool programHalf(uint32_t addr, uint16_t value) {
  if (!flashWait()) return false;
  FLASH->SR = S_ERR | S_EOP;
  FLASH->CR |= F_PG;
  *(volatile uint16_t*)addr = value;
  const bool done = flashWait();
  FLASH->CR &= ~F_PG;
  const bool ok = done && !(FLASH->SR & S_ERR) && *(volatile uint16_t*)addr == value;
  FLASH->SR = S_ERR | S_EOP;
  return ok;
}

bool programBytes(uint32_t addr, const uint8_t* data, size_t len) {     // len even, addr even
  for (size_t i = 0; i < len; i += 2) {
    const uint16_t v = (uint16_t)(data[i] | (data[i + 1] << 8));
    if (!programHalf(addr + i, v)) return false;
  }
  return true;
}

// ── settings parameter table ───────────────────────────────────────────────────────────────────────
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
  PARAM("manual",   K_U8,  manualToggle, 0,   1),
  PARAM("remote",   K_U8,  remoteStart,  0,   1),
  PARAM("sound",    K_U8,  sound,        0,   2),
  PARAM("bright",   K_U8,  brightness,   5,   100),
  PARAM("display",  K_U8,  displayOn,    0,   1),
  { "out1", K_U8, (uint16_t)(offsetof(Settings, outMode) + 0), 0, pc::OM_LAST },
  { "out2", K_U8, (uint16_t)(offsetof(Settings, outMode) + 1), 0, pc::OM_LAST },
  { "out3", K_U8, (uint16_t)(offsetof(Settings, outMode) + 2), 0, pc::OM_LAST },
  { "out4", K_U8, (uint16_t)(offsetof(Settings, outMode) + 3), 0, pc::OM_LAST },
  PARAM("v12cal",   K_U16, v12Cal,       500, 2000),
  PARAM("accal",    K_U16, acCal,        200, 5000),
};
const int NPARAMS = sizeof(PARAMS) / sizeof(PARAMS[0]);

const uint32_t SETTINGS_MAGIC = 0x33534350UL;      // "PCS3"
const uint16_t SETTINGS_VERSION = 5;

uint32_t settingsCrc(const Settings& s) { return pc::crc32((const uint8_t*)&s, offsetof(Settings, crc)); }

// lifetime log: 128 records of 8 bytes {value, value ^ KEY}
const uint32_t LOG_KEY = 0x5CA1AB1EUL;
const uint32_t LOG_SLOTS = STORE_PAGE_SIZE / 8;
uint32_t g_lifetime = 0;
uint32_t g_lifetimeSaved = 0;
int32_t  g_logNext = 0;                 // next free slot, -1 = page must be erased first

bool slotErased(uint32_t i) {
  const volatile uint32_t* p = (const volatile uint32_t*)(STORE_LOG_ADDR + i * 8);
  return p[0] == 0xFFFFFFFFUL && p[1] == 0xFFFFFFFFUL;
}
bool slotValid(uint32_t i, uint32_t& value) {
  const volatile uint32_t* p = (const volatile uint32_t*)(STORE_LOG_ADDR + i * 8);
  if ((p[0] ^ LOG_KEY) != p[1]) return false;
  value = p[0];
  return true;
}
}  // namespace

Settings store::cfg;

namespace store {

void defaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = SETTINGS_MAGIC;
  cfg.version = SETTINGS_VERSION;
  cfg.length = sizeof(Settings);
  pc::copyStr(cfg.name, "press-1", sizeof(cfg.name));
  cfg.minPulseMs = 10;
  cfg.idleMin = 10;
  cfg.stuckSec = 60;
  cfg.tempOffset10 = 0;
  cfg.overTemp10 = 700;
  cfg.manualToggle = 1;
  cfg.remoteStart = 0;                 // starting a hydraulic press from the network is off unless the owner enables it
  cfg.sound = 2;
  cfg.brightness = 80;
  cfg.displayOn = 1;
  cfg.v12Cal = 1000;
  cfg.acCal = 1000;
}

bool load() {
  // lifetime counter: last valid record before the first erased slot
  g_logNext = 0;
  uint32_t v = 0;
  for (uint32_t i = 0; i < LOG_SLOTS; i++) {
    if (slotErased(i)) { g_logNext = (int32_t)i; break; }
    if (slotValid(i, v)) g_lifetime = v;
    g_logNext = (int32_t)i + 1;
  }
  if (g_logNext >= (int32_t)LOG_SLOTS) g_logNext = -1;
  g_lifetimeSaved = g_lifetime;

  const Settings* flash = (const Settings*)STORE_SETTINGS_ADDR;
  if (flash->magic == SETTINGS_MAGIC && flash->version == SETTINGS_VERSION && flash->length == sizeof(Settings) &&
      flash->crc == settingsCrc(*flash)) {
    memcpy(&cfg, flash, sizeof(cfg));
    cfg.name[NAME_LEN - 1] = '\0';
    return true;
  }
  defaults();
  return false;
}

bool save() {
  cfg.magic = SETTINGS_MAGIC;
  cfg.version = SETTINGS_VERSION;
  cfg.length = sizeof(Settings);
  cfg.crc = settingsCrc(cfg);
  // nothing to do if the stored copy is identical
  if (memcmp((const void*)STORE_SETTINGS_ADDR, &cfg, sizeof(cfg)) == 0) return true;
  flashUnlock();
  bool ok = erasePage(STORE_SETTINGS_ADDR);
  if (ok) ok = programBytes(STORE_SETTINGS_ADDR, (const uint8_t*)&cfg, (sizeof(cfg) + 1) & ~1u);
  flashLock();
  return ok;
}

bool setParam(const char* key, const char* value) {
  for (int i = 0; i < NPARAMS; i++) {
    const Param& p = PARAMS[i];
    if (strcmp(p.key, key) != 0) continue;
    uint8_t* base = (uint8_t*)&cfg + p.off;
    if (p.kind == K_STR) {
      char tmp[NAME_LEN];
      pc::sanitize(tmp, sizeof(tmp), value);
      for (char* c = tmp; *c; c++) if (*c == ' ') *c = '-';      // names end up in tags / topics: no blanks
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
    // tiny itoa
    char tmp[12]; int n = 0; bool neg = v < 0; uint32_t u = neg ? (uint32_t)(-v) : (uint32_t)v;
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (neg) tmp[n++] = '-';
    size_t o = 0;
    while (n && o + 1 < outSize) out[o++] = tmp[--n];
    out[o] = '\0';
    return true;
  }
  return false;
}

int paramCount() { return NPARAMS; }
const char* paramKey(int index) { return (index >= 0 && index < NPARAMS) ? PARAMS[index].key : ""; }

uint32_t lifetime() { return g_lifetime; }
void lifetimeAdd(uint32_t n) { g_lifetime += n; }

void lifetimeFlush(bool force) {
  const uint32_t pending = g_lifetime - g_lifetimeSaved;
  if (pending == 0 || (!force && pending < LIFETIME_SAVE_EVERY)) return;
  flashUnlock();
  bool ok = true;
  if (g_logNext < 0) { ok = erasePage(STORE_LOG_ADDR); g_logNext = 0; }
  if (ok) {
    uint32_t rec[2] = { g_lifetime, g_lifetime ^ LOG_KEY };
    ok = programBytes(STORE_LOG_ADDR + (uint32_t)g_logNext * 8, (const uint8_t*)rec, 8);
    if (ok) { g_logNext++; if (g_logNext >= (int32_t)LOG_SLOTS) g_logNext = -1; }
  }
  flashLock();
  if (ok) g_lifetimeSaved = g_lifetime;
}

}  // namespace store
