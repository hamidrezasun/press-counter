// pc_menu.cpp - the two-button menu: update, log, WiFi setup, settings.
//
//   short CALL SUP   next entry / down / decrease
//   short CALL NET   select / up / increase
//   hold  CALL SUP   back / leave the menu
//   hold  CALL NET   save the setting and go back
#include "pc_menu.h"
#include "pc_buttons.h"
#include "pc_config.h"
#include "pc_display.h"
#include "pc_log.h"
#include "pc_net.h"
#include "pc_press.h"
#include "pc_proto.h"
#include "pc_session.h"
#include "pc_store.h"
#include "pc_update.h"

namespace {
enum Screen : uint8_t { S_ROOT, S_UPDATE, S_LOG, S_PARAM, S_EDIT };
Screen g_screen = S_ROOT;
bool   g_open = false;
int    g_sel = 0, g_top = 0, g_logTop = 0;
int    g_param = 0;
char   g_orig[40];
uint32_t g_supAt = 0, g_netAt = 0;
bool g_supDown = false, g_netDown = false, g_supLong = false, g_netLong = false;
uint32_t g_guard = 0;                       // ignore button edges until this millis() (opening / screen change)

const char* const ITEMS[] = {"Check update", "Install update", "Show log", "WiFi setup", "Settings", "Exit"};
const char* const ITEM_DESC[] = {"ask the update server", "download and restart", "last log lines", "open setup network", "machine settings", "back to the counter"};
const int NITEMS = sizeof(ITEMS) / sizeof(ITEMS[0]);

const int ROWS = 6;                              // 64 px / 10 px per row, title bar on top
const int ROOT_ROWS = 4;                         // root list keeps room for the description + hints

char g_hint[80] = "";                           // the full text; longer than the screen scrolls

// Bottom line.  A text that does not fit walks across the screen like a subtitle, so the whole
// instruction is readable on the 21-character OLED line.  Called from draw(), which runs every frame.
void drawHint() {
  const int n = (int)strlen(g_hint);
  char b[22];
  if (n <= 20) {
    snprintf(b, sizeof(b), "%-20.20s", g_hint);
  } else {
    const int gap = 6, total = n + gap;                     // blank gap between the passes
    const int off = (int)((millis() / 220UL) % (uint32_t)total);
    for (int i = 0; i < 20; i++) {
      const int idx = (off + i) % total;
      b[i] = idx < n ? g_hint[idx] : ' ';
    }
    b[20] = '\0';
  }
  display::text(0, 56, b, 1, true);
}

void hint(const char* s) {
  pc::copyStr(g_hint, s, sizeof(g_hint));
  drawHint();
}

void title(const char* t) {
  char b[22];
  snprintf(b, sizeof(b), "%-20.20s", t);
  display::text(0, 0, b, 1, true);
  if (g_screen != S_ROOT) {                     // on the white bar: hold SUP = back, always visible
    char h[12];
    pc::copyStr(h, "hSUP=out", sizeof(h));
    display::text(128 - display::textWidth(h, 1), 0, h, 1, true);
  }
}

void row(int i, const char* t, bool mark) {
  const int y = 10 + i * 9;
  char b[24];
  snprintf(b, sizeof(b), "%c%-19.19s", mark ? '>' : ' ', t);
  display::text(0, y, b, 1, mark && (millis() / 300) % 2 == 0);      // the selection blinks
}

// ── screens ────────────────────────────────────────────────────────────────────────────────────────
void drawRoot() {
  char h[22];
  snprintf(h, sizeof(h), "MENU   %d/%d", g_sel + 1, NITEMS);
  title(h);
  for (int i = 0; i < ROOT_ROWS; i++) {
    const int idx = g_top + i;
    if (idx >= NITEMS) break;
    row(i, ITEMS[idx], idx == g_sel);
  }
  char d[24];
  snprintf(d, sizeof(d), "%s", ITEM_DESC[g_sel]);
  display::text(0, 46, d, 1, false);
  hint("NET=ok  SUP=next  hold SUP=exit");
}

void drawUpdate() {
  const fwupd::Info& u = fwupd::info();
  title("UPDATE");
  char b[22];
  snprintf(b, sizeof(b), "now  %s", PC_FW_VERSION_STR); row(0, b, false);
  snprintf(b, sizeof(b), "new  %s", u.latest[0] ? u.latest : "-"); row(1, b, false);
  snprintf(b, sizeof(b), "%3u%% %s", u.pct, u.phase == fwupd::P_IDLE ? "" : ".."); row(2, b, false);
  row(3, u.msg[0] ? u.msg : "press NET to start", true);
  hint("NET=start the update  hold SUP=exit");
}

void drawLog() {
  title("LOG");
  const int n = plog::count();
  for (int i = 0; i < ROWS - 1; i++) {
    const int idx = n - 1 - g_logTop - (ROWS - 2 - i);
    row(i, idx >= 0 ? plog::line(idx) : "", false);
  }
  hint("SUP/NET=scroll  hold SUP=exit");
}

void drawParam(bool editing) {
  title(editing ? "EDIT" : "SETTINGS");
  if (editing) {
    char v[30];
    store::getParam(store::paramKey(g_param), v, sizeof(v));
    char b[24];
    snprintf(b, sizeof(b), "%-12.12s %.7s", store::paramKey(g_param), v); row(0, b, true);
    snprintf(b, sizeof(b), "was %.16s", g_orig);                  row(1, b, false);
    snprintf(b, sizeof(b), "now %.16s", v);                       row(2, b, false);
    row(4, "NET=+1   SUP=-1", false);
    hint("hold NET=save and go back   hold SUP=cancel");
    return;
  }
  for (int i = 0; i < ROWS - 1; i++) {
    const int idx = g_top + i;
    if (idx >= store::paramCount()) break;
    char v[30], b[24];
    store::getParam(store::paramKey(idx), v, sizeof(v));
    snprintf(b, sizeof(b), "%-8.8s %.11s", store::paramKey(idx), v);      // 20 characters: fits 128 px
    row(i, b, idx == g_param);
  }
  hint("NET=edit  SUP=next  hold SUP=exit");
}

void draw() {
  display::clear();
  switch (g_screen) {
    case S_ROOT: drawRoot(); break;
    case S_UPDATE: drawUpdate(); break;
    case S_LOG: drawLog(); break;
    case S_PARAM: drawParam(false); break;
    case S_EDIT: drawParam(true); break;
  }
  display::flush();
}

void clampScroll() {
  const int rows = (g_screen == S_ROOT) ? ROOT_ROWS : ROWS;
  if (g_sel < g_top) g_top = g_sel;
  if (g_sel >= g_top + rows) g_top = g_sel - rows + 1;
}

void enter() {
  g_guard = millis() + 300;
  switch (g_screen) {
    case S_ROOT:
      if (g_sel == 0) { fwupd::request(false); g_screen = S_UPDATE; }
      else if (g_sel == 1) { fwupd::request(true); g_screen = S_UPDATE; }
      else if (g_sel == 2) { g_screen = S_LOG; g_logTop = 0; }
      else if (g_sel == 3) { net::startSetupAp(); }
      else if (g_sel == 4) { g_screen = S_PARAM; g_param = 0; g_top = 0; }
      else menu::close();
      break;
    case S_PARAM:
      if (store::paramIsText(store::paramKey(g_param))) return;        // text needs the web page
      store::getParam(store::paramKey(g_param), g_orig, sizeof(g_orig));
      g_screen = S_EDIT;
      break;
    default: break;
  }
}

void back() {
  g_guard = millis() + 300;
  if (g_screen == S_EDIT) { g_screen = S_PARAM; return; }             // cancel (nothing saved)
  if (g_screen == S_PARAM) { g_screen = S_ROOT; return; }
  if (g_screen == S_LOG) { g_screen = S_ROOT; return; }
  if (g_screen == S_UPDATE) { g_screen = S_ROOT; return; }
  menu::close();
}
}  // namespace

namespace menu {

void open() {
  g_open = true;
  buttons::setMenuMode(true);          // the two buttons now only drive the menu
  g_guard = millis() + 600;            // whatever was held to get here must not select anything
  g_supDown = buttons::down(1);
  g_netDown = buttons::down(2);
  g_screen = S_ROOT;
  g_sel = g_top = 0;
  session::banner("MENU", 3000);
  plog::log("menu opened (SUP = next/-, NET = ok/+, hold SUP = back)");
}

void close() { g_open = false; buttons::setMenuMode(false); ui::wake(); }

bool active() { return g_open; }

void poll() {
  if (!g_open) return;
  const bool sup = buttons::down(1), net = buttons::down(2);
  const uint32_t now = millis();

  if ((int32_t)(now - g_guard) < 0) {              // still settling: track the state, act on nothing
    g_supDown = sup;
    g_netDown = net;
    ui::wake();
    draw();
    delay(30);
    return;
  }

  if (sup && !g_supDown) { g_supAt = now; g_supLong = false; }
  if (net && !g_netDown) { g_netAt = now; g_netLong = false; }
  if (sup && !g_supLong && now - g_supAt >= 1000) { g_supLong = true; back(); draw(); }
  if (net && !g_netLong && now - g_netAt >= 1000) {
    g_netLong = true;
    if (g_screen == S_EDIT) { store::save(); g_screen = S_PARAM; plog::log("menu: setting saved"); }
  }
  if (!sup && g_supDown && !g_supLong) {                             // short CALL SUP
    if (g_screen == S_ROOT) { g_sel = (g_sel + 1) % NITEMS; clampScroll(); }
    else if (g_screen == S_PARAM) { g_param = (g_param + 1) % store::paramCount(); clampScroll(); if (g_param < g_top) g_top = g_param; }
    else if (g_screen == S_EDIT) store::stepParam(store::paramKey(g_param), -1);
    else if (g_screen == S_LOG) g_logTop++;
  }
  if (!net && g_netDown && !g_netLong) {                             // short CALL NET
    if (g_screen == S_EDIT) store::stepParam(store::paramKey(g_param), +1);
    else if (g_screen == S_LOG) { if (g_logTop > 0) g_logTop--; }
    else enter();
  }
  g_supDown = sup;
  g_netDown = net;

  ui::wake();
  draw();
  delay(30);                                                          // the menu owns the display
}

}  // namespace menu
