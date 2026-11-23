// display.h - minimal driver for the 2.4in ST7789 SPI module (240x320, 4-wire SPI, RGB565): filled rectangles and 5x7 text.
//
// Replaces the Adafruit GFX / ST7789 / BusIO libraries: the Arduino build links every library object (--whole-archive), which dragged the
// I2C and timer HAL into the image and made it larger than the 64 KB of the STM32F103C8.  The UI only needs fillScreen / fillRect / text.
#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

namespace display {
void begin();                                       // reset, initialise, landscape orientation, black screen
int16_t width();                                    // 320 in landscape (DISPLAY_NATIVE_H in portrait ...)
int16_t height();                                   // 240
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t rgb565);
void fillScreen(uint16_t rgb565);
// Text in 6x8 pixel cells (5x7 glyph + spacing), each pixel `scale` x `scale`.  `padTo` > strlen(s): clear the rest of the field with spaces.
void text(int16_t x, int16_t y, const char* s, uint8_t scale, uint16_t fg, uint16_t bg, uint8_t padTo = 0);
}  // namespace display

#endif
