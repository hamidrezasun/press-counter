#include "display.h"
#include <SPI.h>
#include <string.h>
#include "font5x7.h"

// ST7789 commands (datasheet section 9)
#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_NORON   0x13
#define CMD_INVON   0x21
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_RASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_COLMOD  0x3A

namespace {
const SPISettings kSpi(DISPLAY_SPI_HZ, MSBFIRST, SPI_MODE0);
int16_t g_w = DISPLAY_NATIVE_W, g_h = DISPLAY_NATIVE_H;
const uint8_t MAX_SCALE = 6;

inline void selectOn() { digitalWrite(PIN_TFT_CS, LOW); }
inline void selectOff() { digitalWrite(PIN_TFT_CS, HIGH); }

void command(uint8_t c) {
  digitalWrite(PIN_TFT_DC, LOW);
  SPI.transfer(c);
  digitalWrite(PIN_TFT_DC, HIGH);
}

void commandData(uint8_t c, const uint8_t* d, uint8_t n) {
  SPI.beginTransaction(kSpi);
  selectOn();
  command(c);
  for (uint8_t i = 0; i < n; i++) SPI.transfer(d[i]);
  selectOff();
  SPI.endTransaction();
}

void window(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {      // CS already low, inside a transaction
  const uint8_t cx[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
  const uint8_t ry[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
  command(CMD_CASET);
  for (uint8_t i = 0; i < 4; i++) SPI.transfer(cx[i]);
  command(CMD_RASET);
  for (uint8_t i = 0; i < 4; i++) SPI.transfer(ry[i]);
  command(CMD_RAMWR);
}

void burst(const uint8_t* buf, size_t n) {                          // pixel bytes out; the buffer is not modified (transmit only)
  SPI.transfer((void*)buf, n, SPI_TRANSMITONLY);
}
}  // namespace

namespace display {

void begin() {
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_TFT_DC, OUTPUT);
  digitalWrite(PIN_TFT_DC, HIGH);
  pinMode(PIN_TFT_RST, OUTPUT);
  SPI.begin();
  digitalWrite(PIN_TFT_RST, HIGH);                                  // hardware reset pulse (the module also has an RC reset)
  delay(10);
  digitalWrite(PIN_TFT_RST, LOW);
  delay(20);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(150);

  static const uint8_t colmod = 0x55;                               // 16 bit / pixel (RGB565)
  static const uint8_t madctl = DISPLAY_MADCTL;
  commandData(CMD_SWRESET, nullptr, 0);
  delay(150);
  commandData(CMD_SLPOUT, nullptr, 0);
  delay(120);
  commandData(CMD_COLMOD, &colmod, 1);
  delay(10);
  commandData(CMD_MADCTL, &madctl, 1);
#if DISPLAY_INVERT
  commandData(CMD_INVON, nullptr, 0);                               // the IPS panels of these modules need display inversion
  delay(10);
#endif
  commandData(CMD_NORON, nullptr, 0);
  delay(10);
  g_w = (DISPLAY_MADCTL & 0x20) ? DISPLAY_NATIVE_H : DISPLAY_NATIVE_W;   // MV bit: rows and columns exchanged (landscape)
  g_h = (DISPLAY_MADCTL & 0x20) ? DISPLAY_NATIVE_W : DISPLAY_NATIVE_H;
  fillScreen(0x0000);                                               // black before the panel is switched on: no snow
  commandData(CMD_DISPON, nullptr, 0);
  delay(20);
}

int16_t width() { return g_w; }
int16_t height() { return g_h; }

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > g_w) w = g_w - x;
  if (y + h > g_h) h = g_h - y;
  if (w <= 0 || h <= 0) return;
  uint8_t line[64];
  for (uint8_t i = 0; i < sizeof(line); i += 2) { line[i] = (uint8_t)(c >> 8); line[i + 1] = (uint8_t)c; }
  SPI.beginTransaction(kSpi);
  selectOn();
  window(x, y, x + w - 1, y + h - 1);
  uint32_t left = (uint32_t)w * (uint32_t)h * 2UL;
  while (left) {
    const size_t n = left > sizeof(line) ? sizeof(line) : left;
    burst(line, n);
    left -= n;
  }
  selectOff();
  SPI.endTransaction();
}

void fillScreen(uint16_t c) { fillRect(0, 0, g_w, g_h, c); }

namespace {
void glyph(int16_t x, int16_t y, char ch, uint8_t scale, uint16_t fg, uint16_t bg) {
  if (ch < 32 || ch > 126) ch = '?';
  const uint8_t* col = &FONT5X7[(ch - 32) * 5];
  uint8_t line[6 * MAX_SCALE * 2];
  SPI.beginTransaction(kSpi);
  selectOn();
  window(x, y, x + 6 * scale - 1, y + 8 * scale - 1);
  for (uint8_t row = 0; row < 8; row++) {
    size_t n = 0;
    for (uint8_t c = 0; c < 6; c++) {
      const bool on = c < 5 && row < 7 && ((col[c] >> row) & 1);
      const uint16_t px = on ? fg : bg;
      for (uint8_t s = 0; s < scale; s++) { line[n++] = (uint8_t)(px >> 8); line[n++] = (uint8_t)px; }
    }
    for (uint8_t s = 0; s < scale; s++) burst(line, n);
  }
  selectOff();
  SPI.endTransaction();
}
}  // namespace

void text(int16_t x, int16_t y, const char* s, uint8_t scale, uint16_t fg, uint16_t bg, uint8_t padTo) {
  if (scale < 1) scale = 1;
  if (scale > MAX_SCALE) scale = MAX_SCALE;
  size_t n = 0;
  for (; s[n]; n++) {
    if (x + 6 * scale > g_w) return;                                // no wrapping: cut at the right edge
    glyph(x, y, s[n], scale, fg, bg);
    x += 6 * scale;
  }
  for (; n < padTo; n++) {
    if (x + 6 * scale > g_w) return;
    glyph(x, y, ' ', scale, fg, bg);
    x += 6 * scale;
  }
}

}  // namespace display
