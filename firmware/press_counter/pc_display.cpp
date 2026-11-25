#include "pc_display.h"
#include <Wire.h>
#include "pc_font5x7.h"
#include "pc_menu.h"
#include "pc_store.h"
#include "pc_errors.h"
#include "pc_session.h"
#include "pc_press.h"
#include "pc_temp.h"
#include "pc_nfc.h"
#include "pc_net.h"
#include "pc_mqtt.h"
#include "pc_update.h"
#include "pc_buttons.h"

// ── SSD1306 driver ───────────────────────────────────────────────────────────────────────────────────────────────
namespace {
const int W = 128, H = 64;
uint8_t g_fb[W * H / 8];
uint8_t g_addr = 0;

bool cmds(const uint8_t* c, size_t n) {
  Wire.beginTransmission(g_addr);
  Wire.write((uint8_t)0x00);                         // control byte: command stream
  for (size_t i = 0; i < n; i++) Wire.write(c[i]);
  return Wire.endTransmission() == 0;
}
bool cmd1(uint8_t a) { return cmds(&a, 1); }
bool cmd2(uint8_t a, uint8_t b) { const uint8_t c[2] = {a, b}; return cmds(c, 2); }

void setPixel(int x, int y, bool on) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  uint8_t& b = g_fb[x + (y / 8) * W];
  if (on) b |= (uint8_t)(1u << (y & 7)); else b &= (uint8_t)~(1u << (y & 7));
}
}  // namespace

namespace display {

bool begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, OLED_I2C_HZ);
  Wire.setTimeOut(20);
  for (uint8_t a : {(uint8_t)OLED_ADDR, (uint8_t)0x3D}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { g_addr = a; break; }
  }
  if (!g_addr) return false;
  static const uint8_t INIT[] = {
    0xAE,             // display off
    0xD5, 0x80,       // clock divide
    0xA8, 0x3F,       // multiplex 64
    0xD3, 0x00,       // display offset
    0x40,             // start line 0
    0x8D, 0x14,       // charge pump on (the module runs from 3.3 V)
    0x20, 0x00,       // horizontal addressing
    0xA1, 0xC8,       // segment remap + COM scan direction: normal orientation of the common modules
    0xDA, 0x12,       // COM pins
    0x81, 0xCF,       // contrast
    0xD9, 0xF1,       // pre-charge
    0xDB, 0x40,       // VCOMH
    0x2E,             // no scrolling
    0xA4, 0xA6,       // display RAM, not inverted
    0xAF};            // display on
  if (!cmds(INIT, sizeof(INIT))) { g_addr = 0; return false; }
  clear();
  flush();
  return true;
}

bool present() { return g_addr != 0; }
void clear() { memset(g_fb, 0, sizeof(g_fb)); }

void fillRect(int x, int y, int w, int h, bool on) {
  for (int i = x; i < x + w; i++)
    for (int j = y; j < y + h; j++) setPixel(i, j, on);
}

void text(int x, int y, const char* s, uint8_t scale, bool inverse) {
  if (inverse) fillRect(x - 1, y - 1, textWidth(s, scale) + 1, 8 * scale + 1, true);
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c < 32 || c > 126) c = '?';
    const uint8_t* g = &FONT5X7[(c - 32) * 5];
    for (int col = 0; col < 5; col++) {
      uint8_t bits = g[col];
      for (int row = 0; row < 8; row++, bits >>= 1) {
        if (!(bits & 1)) continue;
        for (int dx = 0; dx < scale; dx++)
          for (int dy = 0; dy < scale; dy++) setPixel(x + col * scale + dx, y + row * scale + dy, !inverse);
      }
    }
    x += 6 * scale;
    if (x >= W) break;
  }
}

int textWidth(const char* s, uint8_t scale) { return (int)strlen(s) * 6 * scale - scale; }

void flush() {
  if (!g_addr) return;
  static const uint8_t WIN[] = {0x21, 0x00, 0x7F, 0x22, 0x00, 0x07};       // whole screen
  if (!cmds(WIN, sizeof(WIN))) return;
  for (size_t i = 0; i < sizeof(g_fb); i += 32) {                           // the ESP32 Wire buffer holds 128 bytes; 32 is safe
    Wire.beginTransmission(g_addr);
    Wire.write((uint8_t)0x40);                                              // control byte: data
    Wire.write(g_fb + i, 32);
    if (Wire.endTransmission() != 0) return;
  }
}

void contrast(uint8_t percent) { if (g_addr) cmd2(0x81, (uint8_t)(percent >= 100 ? 0xFF : percent * 255 / 100)); }
void power(bool on) { if (g_addr) cmd1(on ? 0xAF : 0xAE); }

}  // namespace display

// ── screen content ─────────────────────────────────────────────────────────────────────────────────────────────
namespace {
uint32_t g_drawAt = 0, g_wakeAt = 0, g_retryAt = 0;
bool g_dim = false;

void centered(int y, const char* s, uint8_t scale = 1, bool inverse = false) {
  int x = (128 - display::textWidth(s, scale)) / 2;
  display::text(x < 0 ? 0 : x, y, s, scale, inverse);
}

void draw() {
  char b[40];
  display::clear();
  const fwupd::Info& up = fwupd::info();

  // top line: name + network state
  pc::copyStr(b, store::cfg.name, 12);
  display::text(0, 0, b);
  const char* netTxt = net::apMode() ? "SETUP" : !net::wifiUp() ? "NO WIFI" : !mqtt::configured() ? "WIFI" : mqtt::connected() ? "MQTT" : "NO MQTT";
  display::text(128 - display::textWidth(netTxt), 0, netTxt, 1, !net::wifiUp() || (mqtt::configured() && !mqtt::connected()));
  display::fillRect(0, 9, 128, 1, true);

  if (net::apMode() && !net::wifiUp()) {                                 // WiFi setup: show how to reach the page
    centered(14, "WIFI SETUP");
    centered(26, net::apSsid());
    centered(36, "pw: presscounter");
    centered(46, "http://192.168.4.1");
    display::flush();
    return;
  }
  if (up.phase >= fwupd::P_CHECK && up.phase <= fwupd::P_FLASH) {        // firmware update running
    centered(20, "UPDATE", 2);
    snprintf(b, sizeof(b), "%u %%", up.pct);
    centered(42, b);
    display::flush();
    return;
  }

  // the counter
  snprintf(b, sizeof(b), "%lu", (unsigned long)press::count());
  const uint8_t scale = strlen(b) <= 5 ? 3 : 2;
  centered(11, b, scale);

  // operator (left, inverted while the machine is enabled) and temperature (right)
  if (session::operatorId()[0]) snprintf(b, sizeof(b), "OP %s", session::operatorId());
  else snprintf(b, sizeof(b), "OP -");
  b[21] = '\0';
  display::text(0, 37, b, 1, session::relayOn());
  const int16_t t = temp::tempC10();
  if (t != INT16_MIN) snprintf(b, sizeof(b), "%s%d.%dC", t < 0 ? "-" : "", abs(t) / 10, abs(t) % 10);
  else snprintf(b, sizeof(b), "--.-C");
  display::text(128 - display::textWidth(b), 37, b);

  // die id (inverted while a die try-out is running)
  if (session::dieTest()) snprintf(b, sizeof(b), "DIE TEST %s", session::dieId()[0] ? session::dieId() : "");
  else if (session::dieId()[0]) snprintf(b, sizeof(b), "DIE %s", session::dieId());
  else snprintf(b, sizeof(b), "DIE -");
  b[21] = '\0';
  display::text(0, 46, b, 1, session::dieTest());

  // bottom line: banner > error > call > write-tag prompt > die / IP
  const char* banner = session::bannerText();
  const uint8_t e = err::top();
  if (banner[0]) pc::copyStr(b, banner, 22);
  else if (e && pc::errorSeverity(e) > 0) snprintf(b, sizeof(b), "E%02u %s", (unsigned)e, pc::errorText(e));
  else if (session::callMask()) snprintf(b, sizeof(b), "CALL %s%s", session::callMask() & 2 ? "SUPERVISOR " : "", session::callMask() & 4 ? "NET" : "");
  else if (nfc::writePending()) snprintf(b, sizeof(b), "HOLD CARD TO WRITE %lu", (unsigned long)(nfc::writeRemainingMs() / 1000UL));
  else snprintf(b, sizeof(b), "%s", net::wifiUp() ? net::ip() : "");
  b[21] = '\0';
  centered(56, b, 1, (e && pc::errorSeverity(e) >= 2) || session::callMask());
  display::flush();
}
}  // namespace

namespace ui {

void begin() {
  if (!store::cfg.displayOn) return;
  const bool ok = display::begin();
  err::set(pc::ERR_DISPLAY, !ok);
  if (ok) {
    display::contrast(store::cfg.brightness);
    centered(16, "PRESS COUNTER", 1);
    centered(30, "v" PC_FW_VERSION_STR, 1);
    centered(46, "starting...", 1);
    display::flush();
  }
  g_wakeAt = millis();
}

void wake() {
  g_wakeAt = millis();
  if (g_dim) { g_dim = false; display::contrast(store::cfg.brightness); }
}

void poll() {
  if (menu::active()) return;                                           // the menu draws the display itself
  if (!store::cfg.displayOn) { err::set(pc::ERR_DISPLAY, false); return; }
  const uint32_t now = millis();
  if (!display::present()) {                                            // module missing / plugged in later: look again now and then
    if (now - g_retryAt > 10000UL) { g_retryAt = now; if (display::begin()) { err::set(pc::ERR_DISPLAY, false); display::contrast(store::cfg.brightness); } else err::set(pc::ERR_DISPLAY, true); }
    return;
  }
  if (now - g_drawAt < UI_REFRESH_MS) return;
  g_drawAt = now;
  const uint32_t act = buttons::lastActivityMs();
  if ((int32_t)(act - g_wakeAt) > 0) g_wakeAt = act;
  if (press::active() || session::callMask() || err::top()) g_wakeAt = now;
  const bool dim = now - g_wakeAt > DISPLAY_DIM_MS;
  if (dim != g_dim) { g_dim = dim; display::contrast(dim ? 5 : store::cfg.brightness); }
  draw();
}

}  // namespace ui
