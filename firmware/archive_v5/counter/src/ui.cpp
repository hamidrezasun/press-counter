#include "ui.h"
#include "display.h"
#include "store.h"
#include "clock.h"
#include "session.h"
#include "press.h"
#include "temp.h"
#include "errors.h"
#include "events.h"
#include "link.h"
#include "nfc.h"
#include "io.h"
#include "buttons.h"
#include "bootloader.h"

namespace {
const uint16_t C_BLACK = 0x0000, C_WHITE = 0xFFFF, C_RED = 0xF800, C_GREEN = 0x07E0, C_YELLOW = 0xFFE0, C_CYAN = 0x07FF,
               C_GRAY = 0x8410, C_DARK = 0x2104, C_NAVY = 0x0010, C_ORANGE = 0xFD20, C_BLUE = 0x041F;

int16_t  W = 160, H = 128;
uint8_t  TS = 1;                                    // text size multiplier
inline int16_t sx(int16_t x) { return (int16_t)((int32_t)x * W / 160); }
inline int16_t sy(int16_t y) { return (int16_t)((int32_t)y * H / 128); }

enum Page : uint8_t { P_HOME, P_SESSION, P_NET, P_ERRORS, P_INFO, P_COUNT };
enum Mode : uint8_t { M_PAGES, M_MENU, M_EDIT, M_CONFIRM };
Page     g_page = P_HOME;
Mode     g_mode = M_PAGES;
bool     g_full = true;                              // redraw everything
bool     g_dimmed = false;
bool     g_active = false;                           // a display is fitted and initialised (setting 'display'); without it only the buttons, LEDs and buzzer work
uint32_t g_lastDraw = 0;
bool     g_cancelSupDone = false, g_cancelNetDone = false;

// ── drawing helpers ───────────────────────────────────────────────────────────────────────────
void put(int16_t lx, int16_t ly, const char* s, uint8_t size, uint16_t fg, uint16_t bg, uint8_t padTo = 0) {
  display::text(sx(lx), sy(ly), s, (uint8_t)(size * TS), fg, bg, padTo);
}
void fillRect(int16_t lx, int16_t ly, int16_t lw, int16_t lh, uint16_t c) { display::fillRect(sx(lx), sy(ly), sx(lw), sy(lh), c); }

void fmtTemp(char* b, size_t n, int16_t t10) {
  if (t10 == INT16_MIN) { pc::copyStr(b, "--.-C", n); return; }
  snprintf(b, n, "%d.%dC", t10 / 10, (t10 < 0 ? -t10 : t10) % 10);
}
void fmtHms(char* b, size_t n, uint32_t s) { snprintf(b, n, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60), (unsigned long)(s % 60)); }
void fmtDur(char* b, size_t n, uint32_t ms) { snprintf(b, n, "%lu.%02lus", (unsigned long)(ms / 1000), (unsigned long)((ms % 1000) / 10)); }

void header(const char* title) {
  fillRect(0, 0, 160, 11, C_NAVY);
  put(2, 2, title, 1, C_WHITE, C_NAVY);
}

// ── pages ─────────────────────────────────────────────────────────────────────────────────────
struct HomeCache {
  uint32_t count = 0xFFFFFFFFUL;
  char line[8][28];
  uint16_t col[8];
} g_home;

void setLine(uint8_t i, const char* s, uint16_t col, int16_t ly, uint8_t size, uint16_t bg, uint8_t pad) {
  if (strncmp(g_home.line[i], s, sizeof(g_home.line[i]) - 1) == 0 && g_home.col[i] == col && !g_full) return;
  pc::copyStr(g_home.line[i], s, sizeof(g_home.line[i]));
  g_home.col[i] = col;
  put(2, ly, s, size, col, bg, pad);
}

void drawHome() {
  if (g_full) {
    display::fillScreen(C_BLACK);
    header(store::cfg.name);
    put(2, 56, "STROKES", 1, C_GRAY, C_BLACK);
    memset(g_home.line, 0, sizeof(g_home.line));
    g_home.count = 0xFFFFFFFFUL;
  }
  // header: name left, status letters right (W wifi/server, R rfid, L link to the gateway chip)
  {
    fillRect(100, 0, 60, 11, C_NAVY);
    const bool wifi = link::wifiUp(), srv = link::serverUp();
    put(124, 2, "W", 1, srv ? C_GREEN : wifi ? C_YELLOW : C_RED, C_NAVY);
    put(134, 2, "R", 1, nfc::present() ? C_GREEN : C_RED, C_NAVY);
    put(144, 2, "L", 1, link::alive() ? C_GREEN : C_RED, C_NAVY);
  }
  // big stroke counter
  const uint32_t cnt = press::count();
  if (cnt != g_home.count || g_full) {
    g_home.count = cnt;
    char b[12];
    snprintf(b, sizeof(b), "%lu", (unsigned long)cnt);
    fillRect(0, 14, 160, 40, C_BLACK);
    const uint8_t digits = (uint8_t)strlen(b);
    const uint8_t sz = digits <= 4 ? 5 : digits <= 5 ? 4 : 3;
    const int16_t cw = 6 * sz;                                       // logical width of one digit
    put((int16_t)((160 - cw * digits) / 2), 16, b, sz, press::active() ? C_YELLOW : C_WHITE, C_BLACK);
  }
  char b[28];
  if (press::sinceLastStrokeMs() != 0xFFFFFFFFUL) { char d[12]; fmtDur(d, sizeof(d), press::lastDurationMs()); snprintf(b, sizeof(b), "last %s", d); }
  else pc::copyStr(b, "no stroke yet", sizeof(b));
  setLine(0, b, C_GRAY, 56, 1, C_BLACK, 18);
  // state bar
  const bool on = session::relayOn();
  fillRect(0, 66, 160, 14, on ? C_GREEN : C_RED);
  put(4, 69, on ? "RUN" : "STOP", 1, on ? C_BLACK : C_WHITE, on ? C_GREEN : C_RED);
  if (on && session::operatorId()[0]) { snprintf(b, sizeof(b), "op:%.12s", session::operatorId()); put(44, 69, b, 1, C_BLACK, C_GREEN); }
  else if (!on) put(44, 69, "scan card to start", 1, C_WHITE, C_RED);
  // die + temperature
  char t[10];
  fmtTemp(t, sizeof(t), temp::tempC10());
  snprintf(b, sizeof(b), "die:%.10s %s", session::dieId()[0] ? session::dieId() : "-", t);
  setLine(1, b, C_CYAN, 84, 1, C_BLACK, 26);
  // session time / banner
  const char* bn = session::bannerText();
  if (bn[0]) setLine(2, bn, C_YELLOW, 96, 1, C_BLACK, 26);
  else if (on) { char h[12]; fmtHms(h, sizeof(h), session::sessionSeconds()); snprintf(b, sizeof(b), "session %s", h); setLine(2, b, C_GRAY, 96, 1, C_BLACK, 26); }
  else if (nfc::writePending()) { snprintf(b, sizeof(b), "WRITE: present card %lus", (unsigned long)(nfc::writeRemainingMs() / 1000UL)); setLine(2, b, C_YELLOW, 96, 1, C_BLACK, 26); }
  else setLine(2, "", C_GRAY, 96, 1, C_BLACK, 26);
  // footer: error / call / hint
  const uint8_t top = err::top();
  if (top) { snprintf(b, sizeof(b), "E%02u %s", top, pc::errorText(top)); setLine(3, b, pc::errorSeverity(top) >= 2 ? C_RED : C_ORANGE, 112, 1, C_BLACK, 26); }
  else if (session::callMask()) {
    snprintf(b, sizeof(b), "CALL: %s%s", (session::callMask() & 2) ? "SUP " : "", (session::callMask() & 4) ? "NET" : "");
    setLine(3, b, C_ORANGE, 112, 1, C_BLACK, 26);
  } else setLine(3, "< > page   OK menu", C_GRAY, 112, 1, C_BLACK, 26);
  g_full = false;
}

void drawSession() {
  if (g_full) { display::fillScreen(C_BLACK); header("SESSION"); }
  char b[28], h[12], d[12], t[10];
  fmtHms(h, sizeof(h), session::sessionSeconds());
  snprintf(b, sizeof(b), "operator %.12s", session::operatorId()[0] ? session::operatorId() : "-"); put(2, 16, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "die      %.12s", session::dieId()[0] ? session::dieId() : "-");           put(2, 28, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "machine  %s", session::relayOn() ? "ENABLED" : "stopped");                put(2, 40, b, 1, session::relayOn() ? C_GREEN : C_RED, C_BLACK, 26);
  snprintf(b, sizeof(b), "time     %s", h);                                                          put(2, 52, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "strokes  %lu", (unsigned long)press::count());                              put(2, 64, b, 1, C_WHITE, C_BLACK, 26);
  fmtDur(d, sizeof(d), press::lastDurationMs());
  snprintf(b, sizeof(b), "last     %s", d);                                                           put(2, 76, b, 1, C_WHITE, C_BLACK, 26);
  fmtTemp(t, sizeof(t), temp::tempC10());
  snprintf(b, sizeof(b), "temp     %s", t);                                                           put(2, 88, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "lifetime %lu", (unsigned long)store::lifetime());                           put(2, 100, b, 1, C_GRAY, C_BLACK, 26);
  g_full = false;
}

void drawNet() {
  if (g_full) { display::fillScreen(C_BLACK); header("NETWORK"); }
  char b[28];
  snprintf(b, sizeof(b), "gateway  %s", link::alive() ? "linked" : "NO LINK");                         put(2, 16, b, 1, link::alive() ? C_GREEN : C_RED, C_BLACK, 26);
  snprintf(b, sizeof(b), "wifi     %s", link::wifiUp() ? "connected" : "down");                         put(2, 28, b, 1, link::wifiUp() ? C_GREEN : C_RED, C_BLACK, 26);
  snprintf(b, sizeof(b), "ip %s", link::ip()[0] ? link::ip() : "-");                                   put(2, 40, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "rssi %d dBm", link::rssi());                                                 put(2, 52, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "server   %s", link::serverUp() ? "ok" : "no");                               put(2, 64, b, 1, link::serverUp() ? C_GREEN : C_ORANGE, C_BLACK, 26);
  snprintf(b, sizeof(b), "time     %s", link::ntpOk() ? "ntp" : clk::valid() ? "old" : "unset");     put(2, 76, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "events waiting %u", events::pending());                                     put(2, 88, b, 1, events::pending() ? C_ORANGE : C_WHITE, C_BLACK, 26);
  if (link::fwState() >= 1 && link::fwState() <= 3) { snprintf(b, sizeof(b), "UPDATE %u%%", link::fwPercent()); put(2, 112, b, 1, C_YELLOW, C_BLACK, 26); }
  else put(2, 112, "", 1, C_WHITE, C_BLACK, 26);
  g_full = false;
}

void drawErrors() {
  if (g_full) { display::fillScreen(C_BLACK); header("ERRORS / WARNINGS"); }
  uint8_t row = 0;
  char b[28];
  for (uint8_t c = 1; c <= pc::ERR_LAST && row < 9; c++) {
    if (!err::active(c)) continue;
    snprintf(b, sizeof(b), "E%02u %s", c, pc::errorText(c));
    put(2, (int16_t)(16 + row * 11), b, 1, pc::errorSeverity(c) >= 2 ? C_RED : C_ORANGE, C_BLACK, 26);
    row++;
  }
  if (row == 0) { put(2, 16, "no active errors", 1, C_GREEN, C_BLACK, 26); row = 1; }
  for (; row < 9; row++) put(2, (int16_t)(16 + row * 11), "", 1, C_WHITE, C_BLACK, 26);
  put(2, 118, "OK: acknowledge", 1, C_GRAY, C_BLACK, 26);
  g_full = false;
}

void drawInfo() {
  if (g_full) { display::fillScreen(C_BLACK); header("INFO"); }
  char b[28], t[10], m[10], h[12];
  fmtTemp(t, sizeof(t), temp::tempC10());
  fmtTemp(m, sizeof(m), temp::mcuTempC10());
  fmtHms(h, sizeof(h), millis() / 1000UL);
  snprintf(b, sizeof(b), "firmware v%s", PC_FW_VERSION_STR);                                put(2, 16, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "uptime %s", h);                                                   put(2, 28, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "vdda %u mV", temp::vddaMilliVolt());                              put(2, 40, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "lm35 %s  mcu %s", t, m);                                          put(2, 52, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "noise rejected %lu", (unsigned long)press::rejected());           put(2, 64, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "events lost %lu", (unsigned long)events::dropped());              put(2, 76, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "link bad frames %lu", (unsigned long)link::badFrames());          put(2, 88, b, 1, C_WHITE, C_BLACK, 26);
  snprintf(b, sizeof(b), "name %.20s", store::cfg.name);                                    put(2, 100, b, 1, C_WHITE, C_BLACK, 26);
  g_full = false;
}

// ── menu ──────────────────────────────────────────────────────────────────────────────────────
enum ItemType : uint8_t { IT_ACTION, IT_INT, IT_ENUM, IT_BOOL, IT_SUB };
enum Action : uint8_t { A_NONE, A_TOGGLE, A_RESET, A_CALL_SUP, A_CALL_NET, A_CLEAR, A_CHECK_FW, A_REBOOT, A_ROM, A_DEFAULTS, A_BACK };
struct Item { const char* label; ItemType type; const char* key; int16_t lo, hi, step; const char* names; uint8_t arg; };

const Item MENU_MAIN[] = {
  {"Machine ON/OFF", IT_ACTION, "", 0, 0, 0, "", A_TOGGLE},
  {"Reset counter", IT_ACTION, "", 0, 0, 0, "", A_RESET},
  {"Call supervisor", IT_ACTION, "", 0, 0, 0, "", A_CALL_SUP},
  {"Call net / maint", IT_ACTION, "", 0, 0, 0, "", A_CALL_NET},
  {"Clear errors", IT_ACTION, "", 0, 0, 0, "", A_CLEAR},
  {"Settings", IT_SUB, "", 0, 0, 0, "", 1},
  {"Service", IT_SUB, "", 0, 0, 0, "", 2},
  {"Back", IT_ACTION, "", 0, 0, 0, "", A_BACK},
};
const Item MENU_SET[] = {
  {"Min stroke ms", IT_INT, "minpulse", 1, 5000, 5, "", 0},
  {"Idle off min", IT_INT, "idle", 0, 600, 5, "", 0},
  {"Brightness %", IT_INT, "bright", 5, 100, 5, "", 0},
  {"Sound", IT_ENUM, "sound", 0, 2, 1, "OFF|ERRORS|ALL", 0},
  {"Manual ON/OFF", IT_BOOL, "manual", 0, 1, 1, "NO|YES", 0},
  {"Remote start", IT_BOOL, "remote", 0, 1, 1, "NO|YES", 0},
  {"Display", IT_BOOL, "display", 0, 1, 1, "OFF|ON", 0},
  {"Over temp x0.1C", IT_INT, "overtemp", 300, 1200, 50, "", 0},
  {"Temp offset x0.1C", IT_INT, "tempoff", -100, 100, 1, "", 0},
  {"Back", IT_ACTION, "", 0, 0, 0, "", A_BACK},
};
const Item MENU_SVC[] = {
  {"Check fw update", IT_ACTION, "", 0, 0, 0, "", A_CHECK_FW},
  {"Reboot", IT_ACTION, "", 0, 0, 0, "", A_REBOOT},
  {"ROM bootloader", IT_ACTION, "", 0, 0, 0, "", A_ROM},
  {"Factory defaults", IT_ACTION, "", 0, 0, 0, "", A_DEFAULTS},
  {"Back", IT_ACTION, "", 0, 0, 0, "", A_BACK},
};
const Item* g_menu = MENU_MAIN;
uint8_t g_menuLen = sizeof(MENU_MAIN) / sizeof(MENU_MAIN[0]);
uint8_t g_menuId = 0, g_sel = 0, g_top = 0;
int16_t g_edit = 0;
Action  g_confirmAction = A_NONE;

void openMenu(uint8_t id) {
  g_menuId = id;
  g_menu = id == 0 ? MENU_MAIN : id == 1 ? MENU_SET : MENU_SVC;
  g_menuLen = id == 0 ? sizeof(MENU_MAIN) / sizeof(MENU_MAIN[0]) : id == 1 ? sizeof(MENU_SET) / sizeof(MENU_SET[0]) : sizeof(MENU_SVC) / sizeof(MENU_SVC[0]);
  g_sel = g_top = 0;
  g_mode = M_MENU;
  g_full = true;
}

int16_t itemValue(const Item& it) {
  char v[16];
  if (!store::getParam(it.key, v, sizeof(v))) return 0;
  return (int16_t)pc::toInt(v);
}
void enumName(const char* names, int idx, char* out, size_t n) {
  int i = 0;
  const char* p = names;
  while (i < idx && *p) { if (*p == '|') i++; p++; }
  size_t k = 0;
  while (*p && *p != '|' && k + 1 < n) out[k++] = *p++;
  out[k] = '\0';
}
void itemText(const Item& it, int16_t v, char* out, size_t n) {
  if (it.type == IT_ENUM || it.type == IT_BOOL) { enumName(it.names, v, out, n); return; }
  if (!strcmp(it.key, "overtemp") || !strcmp(it.key, "tempoff")) { snprintf(out, n, "%d.%d", v / 10, (v < 0 ? -v : v) % 10); return; }
  snprintf(out, n, "%d", v);
}

void drawMenu() {
  if (g_full) { display::fillScreen(C_BLACK); header(g_menuId == 0 ? "MENU" : g_menuId == 1 ? "SETTINGS" : "SERVICE"); }
  const uint8_t rows = 9;
  if (g_sel < g_top) g_top = g_sel;
  if (g_sel >= g_top + rows) g_top = (uint8_t)(g_sel - rows + 1);
  for (uint8_t r = 0; r < rows; r++) {
    const uint8_t i = (uint8_t)(g_top + r);
    const int16_t y = (int16_t)(14 + r * 11);
    if (i >= g_menuLen) { fillRect(0, y, 160, 11, C_BLACK); continue; }
    const Item& it = g_menu[i];
    const bool sel = i == g_sel;
    const uint16_t bg = sel ? (g_mode == M_EDIT ? C_ORANGE : C_BLUE) : C_BLACK, fg = sel ? C_WHITE : C_WHITE;
    fillRect(0, y, 160, 11, bg);
    char label[24];
    pc::copyStr(label, it.label, sizeof(label));
    put(2, (int16_t)(y + 2), label, 1, fg, bg);
    if (it.type == IT_SUB) put(148, (int16_t)(y + 2), ">", 1, fg, bg);
    else if (it.type != IT_ACTION) {
      char v[16];
      itemText(it, (sel && g_mode == M_EDIT) ? g_edit : itemValue(it), v, sizeof(v));
      char out[20];
      snprintf(out, sizeof(out), (sel && g_mode == M_EDIT) ? "<%s>" : "%s", v);
      put((int16_t)(158 - 6 * (int16_t)strlen(out)), (int16_t)(y + 2), out, 1, fg, bg);
    }
  }
  if (g_mode == M_CONFIRM) { fillRect(0, 100, 160, 28, C_RED); put(4, 104, "CONFIRM: press OK", 1, C_WHITE, C_RED); put(4, 116, "LEFT = cancel", 1, C_WHITE, C_RED); }
  g_full = false;
}

void runAction(uint8_t a) {
  switch (a) {
    case A_TOGGLE:
      if (store::cfg.manualToggle) { session::toggleRelay(pc::RR_BUTTON); g_mode = M_PAGES; g_full = true; }
      else { session::banner("USE A PERSONNEL CARD"); io::play(io::SND_WARNING); }
      break;
    case A_RESET: session::resetCounter(); session::banner("COUNTER RESET"); g_mode = M_PAGES; g_full = true; break;
    case A_CALL_SUP: session::call(pc::CALL_SUPERVISOR); g_mode = M_PAGES; g_full = true; break;
    case A_CALL_NET: session::call(pc::CALL_NET); g_mode = M_PAGES; g_full = true; break;
    case A_CLEAR: err::acknowledge(); session::banner("ERRORS CLEARED"); g_mode = M_PAGES; g_full = true; break;
    case A_CHECK_FW: link::send("U,check"); session::banner("CHECKING UPDATE"); g_mode = M_PAGES; g_full = true; break;
    case A_REBOOT: g_confirmAction = A_REBOOT; g_mode = M_CONFIRM; break;
    case A_ROM:
      if (session::relayOn()) { session::banner("STOP MACHINE FIRST"); io::play(io::SND_WARNING); g_mode = M_PAGES; g_full = true; }
      else { g_confirmAction = A_ROM; g_mode = M_CONFIRM; }
      break;
    case A_DEFAULTS: g_confirmAction = A_DEFAULTS; g_mode = M_CONFIRM; break;
    case A_BACK: if (g_menuId == 0) { g_mode = M_PAGES; g_full = true; } else openMenu(0); break;
    default: break;
  }
}

void confirmed() {
  switch (g_confirmAction) {
    case A_REBOOT: NVIC_SystemReset(); break;
    case A_ROM: link::send("B,ROM"); delay(30); bootloader::enterRom(); break;
    case A_DEFAULTS: { store::defaults(); store::save(); session::banner("DEFAULTS LOADED"); break; }
    default: break;
  }
  g_confirmAction = A_NONE;
  g_mode = M_PAGES;
  g_full = true;
}

// ── button handling ─────────────────────────────────────────────────────────────────────────────
void onButton(const buttons::Event& e) {
  if (g_dimmed && g_active) { g_dimmed = false; io::setBacklight(store::cfg.brightness); if (e.kind == buttons::PRESS) return; }
  io::play(io::SND_CLICK);

  // call buttons work on every screen: short = call, hold = cancel (see poll)
  if (e.button == buttons::SUP || e.button == buttons::NET) {
    const uint8_t type = e.button == buttons::SUP ? pc::CALL_SUPERVISOR : pc::CALL_NET;
    if (e.kind == buttons::SHORT) {
      if (session::callMask() & (1u << type)) session::banner("CALL ACTIVE - HOLD TO CANCEL");
      else session::call(type);
    }
    return;
  }
  if (!g_active) {                                                    // no display: OK acknowledges the errors, nothing else to navigate
    if (e.button == buttons::OK && e.kind == buttons::PRESS) err::acknowledge();
    return;
  }
  if (g_mode == M_PAGES) {
    if (e.kind != buttons::PRESS && e.kind != buttons::REPEAT) return;
    if (e.button == buttons::RIGHT) { g_page = (Page)((g_page + 1) % P_COUNT); g_full = true; }
    else if (e.button == buttons::LEFT) { g_page = (Page)((g_page + P_COUNT - 1) % P_COUNT); g_full = true; }
    else if (e.button == buttons::OK) {
      if (g_page == P_ERRORS) { err::acknowledge(); g_full = true; }
      else openMenu(0);
    }
    return;
  }
  if (g_mode == M_MENU) {
    if (e.kind != buttons::PRESS && e.kind != buttons::REPEAT) return;
    if (e.button == buttons::UP) { if (g_sel > 0) g_sel--; }
    else if (e.button == buttons::DOWN) { if (g_sel + 1 < g_menuLen) g_sel++; }
    else if (e.button == buttons::LEFT) { if (g_menuId == 0) { g_mode = M_PAGES; g_full = true; } else openMenu(0); }
    else if (e.button == buttons::OK || e.button == buttons::RIGHT) {
      const Item& it = g_menu[g_sel];
      if (it.type == IT_SUB) openMenu(it.arg);
      else if (it.type == IT_ACTION) runAction(it.arg);
      else if (it.type == IT_BOOL) {                                  // toggles at once
        const int16_t v = itemValue(it) ? 0 : 1;
        char s[8]; snprintf(s, sizeof(s), "%d", v);
        if (store::setParam(it.key, s)) store::save();
      } else { g_edit = itemValue(it); g_mode = M_EDIT; }
    }
    return;
  }
  if (g_mode == M_EDIT) {
    if (e.kind != buttons::PRESS && e.kind != buttons::REPEAT) return;
    const Item& it = g_menu[g_sel];
    if (e.button == buttons::UP) { g_edit = (int16_t)(g_edit + it.step); if (g_edit > it.hi) g_edit = it.hi; }
    else if (e.button == buttons::DOWN) { g_edit = (int16_t)(g_edit - it.step); if (g_edit < it.lo) g_edit = it.lo; }
    else if (e.button == buttons::LEFT) { g_mode = M_MENU; }
    else if (e.button == buttons::OK) {
      char s[12]; snprintf(s, sizeof(s), "%d", g_edit);
      if (store::setParam(it.key, s) && store::save()) {
        if (!strcmp(it.key, "bright")) io::setBacklight(store::cfg.brightness);
        io::play(io::SND_SUCCESS);
      } else io::play(io::SND_ERROR);
      g_mode = M_MENU;
    }
    return;
  }
  if (g_mode == M_CONFIRM) {
    if (e.kind != buttons::PRESS) return;
    if (e.button == buttons::OK) confirmed();
    else if (e.button == buttons::LEFT) { g_mode = M_MENU; g_full = true; }
    return;
  }
}
}  // namespace

namespace ui {

namespace {
void startDisplay() {
  io::setBacklight(store::cfg.brightness);
  display::begin();
  W = display::width();
  H = display::height();
  TS = (uint8_t)(W / 160 < 1 ? 1 : W / 160);
  g_active = true;
  g_full = true;
  g_mode = M_PAGES;
}
}  // namespace

void begin() {
  if (store::cfg.displayOn) startDisplay();
  else io::setBacklight(0);
}

void bootMessage(const char* line) {
  if (!g_active) return;
  static uint8_t row = 0;
  if (row == 0) { display::fillScreen(C_BLACK); put(2, 2, "PRESS COUNTER", 1, C_CYAN, C_BLACK); put(2, 14, "v" PC_FW_VERSION_STR, 1, C_GRAY, C_BLACK); }
  put(2, (int16_t)(30 + (row % 8) * 11), line, 1, C_WHITE, C_BLACK, 26);
  row++;
}

void wake() { g_dimmed = false; if (g_active) io::setBacklight(store::cfg.brightness); }

void poll() {
  buttons::Event e;
  while (buttons::pop(e)) { onButton(e); g_lastDraw = 0; }

  const uint32_t now = millis();
  // hold a call button for 1.5 s to cancel its call
  if (buttons::down(buttons::SUP) && buttons::heldMs(buttons::SUP) >= BTN_CANCEL_CALL_MS) {
    if (!g_cancelSupDone) { g_cancelSupDone = true; session::cancelCall(pc::CALL_SUPERVISOR); }
  } else if (!buttons::down(buttons::SUP)) g_cancelSupDone = false;
  if (buttons::down(buttons::NET) && buttons::heldMs(buttons::NET) >= BTN_CANCEL_CALL_MS) {
    if (!g_cancelNetDone) { g_cancelNetDone = true; session::cancelCall(pc::CALL_NET); }
  } else if (!buttons::down(buttons::NET)) g_cancelNetDone = false;

  // the 'display' setting can change at any time (web page / console / menu)
  if ((store::cfg.displayOn != 0) != g_active) {
    if (store::cfg.displayOn) startDisplay();
    else { g_active = false; io::setBacklight(0); }
  }
  if (!g_active) return;

  // display dimming
  const bool idle = now - buttons::lastActivityMs() > BACKLIGHT_DIM_MS && !err::count() && !session::callMask();
  if (idle && !g_dimmed) { g_dimmed = true; io::setBacklight(15); }
  else if (!idle && g_dimmed) { g_dimmed = false; io::setBacklight(store::cfg.brightness); }

  if (now - g_lastDraw < UI_REFRESH_MS) return;
  g_lastDraw = now;
  if (link::fwState() >= 1 && link::fwState() <= 3) {                    // firmware update running on the gateway side: keep the user informed
    static uint8_t lastPct = 255;
    if (g_full || lastPct != link::fwPercent()) {
      lastPct = link::fwPercent();
      display::fillScreen(C_BLACK);
      put(10, 40, "FIRMWARE UPDATE", 1, C_YELLOW, C_BLACK);
      char b[20]; snprintf(b, sizeof(b), "%u %%", link::fwPercent());
      put(50, 60, b, 2, C_WHITE, C_BLACK);
      put(10, 100, "do not switch off", 1, C_GRAY, C_BLACK);
      g_full = false;
    }
    return;
  }
  if (g_mode == M_PAGES) {
    switch (g_page) {
      case P_HOME: drawHome(); break;
      case P_SESSION: drawSession(); break;
      case P_NET: drawNet(); break;
      case P_ERRORS: drawErrors(); break;
      default: drawInfo(); break;
    }
  } else drawMenu();
}

}  // namespace ui
