// pc_display.h - 0.96in 128x64 SSD1306 OLED on I2C (own small driver: frame buffer + 5x7 font, no libraries).
#ifndef PC_DISPLAY_H
#define PC_DISPLAY_H

#include "pc_config.h"

namespace display {
bool begin();                                  // true if a controller answered (0x3C or 0x3D)
bool present();
void clear();
void text(int x, int y, const char* s, uint8_t scale = 1, bool inverse = false);   // y in pixels (top of the character cell)
int textWidth(const char* s, uint8_t scale = 1);
void fillRect(int x, int y, int w, int h, bool on);
void flush();                                  // sends the frame buffer
void contrast(uint8_t percent);
void power(bool on);
}  // namespace display

namespace ui {
void begin();
void poll();                                   // redraws every UI_REFRESH_MS
void wake();                                   // full brightness again (stroke, card, button)
}  // namespace ui

#endif
