// config.h - pin map and tunables of the counter node (bare STM32F103C8T6 on the v5 MAIN board).
// The nets are named in hardware/tools/design.py (main_board); the panel cable pin map is design.HMI_PINS.
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "pc_proto.h"

// ── pins (STM32 pin names) ───────────────────────────────────────────────────────────────────────
// panel cable (J6): buttons are active LOW with the internal pull-up (1k series resistor on the main board, 100nF on the panel)
#define PIN_BTN_UP       PA0
#define PIN_BTN_DOWN     PA1
#define PIN_BTN_LEFT     PB4
#define PIN_BTN_RIGHT    PB5
#define PIN_BTN_OK       PB10
#define PIN_BTN_SUP      PB3    // CALL SUPERVISOR
#define PIN_BTN_NET      PA15   // CALL NET / MAINTENANCE
#define PIN_TFT_CS       PA4    // SPI1: SCK PA5, MISO PA6, MOSI PA7 (100R series resistors on SCK / MOSI)
#define PIN_TFT_DC       PB14
#define PIN_TFT_RST      PB15
#define PIN_TFT_BL       PB0    // TIM3_CH3 PWM back-light
#define PIN_RC_SS        PB12   // RC522 SDA/SS
#define PIN_RC_RST       PB13
#define PIN_LED_RUN      PB8    // panel, green: machine enabled (470 R on the panel board)
#define PIN_LED_ALERT    PB9    // panel, red: error / call
#define PIN_BUZZER       PA8    // panel: -> SS8050 -> 5 V buzzer, HIGH = on
// main board
#define PIN_PRESS        PB6    // PC817 output, 10k pull-up, LOW = press voltage present (EXTI6)
#define PIN_RELAY        PB7    // -> 1k -> SS8050 -> relay K1 coil, HIGH = relay closed = machine enabled; 10k pull-down while booting
#define PIN_TEMP         PB1    // LM35 via 1k + 100nF (ADC1_IN9)
// output relays OUT1..OUT4 (K2..K5): PB11, PC13, PC14, PC15 (see OUT_PINS in outputs.cpp; PC13-15 may only drive 3 mA: 2.2 k base resistors)
// voltage sense on ADC2 (sense.cpp): PA2 = 12 V input (divider 33k / 10k), PA3 = 230 V AC from a ZMPT101B module
// USART1 (PA9 TX / PA10 RX): link to the ESP8266 gateway.   USB (console): PA11 / PA12.   SWD: PA13 / PA14.
// BOOT1 = PB2 (strapped low, do not use).   No unused pins are left.

#define LINK_BAUD        38400UL

// ── display: 2.4in ST7789 module, 240 x 320, driven by display.cpp (no Adafruit libraries) ───────────
#ifndef DISPLAY_NATIVE_W
#define DISPLAY_NATIVE_W 240         // panel as delivered (portrait)
#define DISPLAY_NATIVE_H 320
#endif
#ifndef DISPLAY_MADCTL
#define DISPLAY_MADCTL   0xA0        // memory access control: MY | MV = landscape 320 x 240, RGB order (0x08 = BGR: use it if red and blue are swapped)
#endif
#ifndef DISPLAY_INVERT
#define DISPLAY_INVERT   1           // IPS panels need display inversion; set 0 if the colours look negative
#endif
#ifndef DISPLAY_SPI_HZ
#define DISPLAY_SPI_HZ   9000000UL   // 100 R series resistors + a ribbon cable: 9 MHz is safe, the panel could do 60 MHz
#endif

// ── timing / behaviour ───────────────────────────────────────────────────────────────────────────
#define BTN_DEBOUNCE_MS          25UL
#define BTN_LONG_MS              900UL
#define BTN_REPEAT_MS            180UL
#define BTN_CANCEL_CALL_MS       1500UL
#define TAG_COOLDOWN_MS          3000UL        // ignore the same card for 3 s after it was handled
#define TAG_WRITE_TIMEOUT_MS     30000UL
#define NFC_POLL_MS              150UL
#define NFC_HEALTH_MS            5000UL
#define TEMP_SAMPLE_MS           500UL
#define TEMP_REPORT_MS           60000UL
#define PRESS_GAP_MS             3UL           // an inactive gap shorter than this inside a pulse is contact bounce
#define PRESS_TRAIN_GAP_MS       1000UL
#define LINK_ALIVE_MS            6000UL        // gateway counts as present if it spoke within this time
#define LINK_ERROR_MS            30000UL       // ... and raises E05 after this long
#define SERVER_ERROR_MS          300000UL      // no WiFi for 5 minutes -> E06
#define STATUS_PERIOD_MS         1000UL        // status snapshot to the gateway
#define EVENT_RETRY_MS           2500UL
#define IWDG_TIMEOUT_MS          4000UL
#define UI_REFRESH_MS            250UL
#define BACKLIGHT_DIM_MS         120000UL      // dim the display after 2 minutes without button / tag
#define LIFETIME_SAVE_EVERY      25UL

#define MAX_TAG_TEXT             38            // fits the three data blocks of one MIFARE Classic sector
#define EVENT_QUEUE_SIZE         48
#define NAME_LEN                 24
#define LINE_PAYLOAD             200           // a link line payload

// ── hardware options ─────────────────────────────────────────────────────────────────────────────
#ifndef RELAY_ON_LEVEL
#define RELAY_ON_LEVEL HIGH
#endif

// Settings + lifetime counter live in the last two 1 KB pages of the first 64 KB (valid on every F103C8/CB).
#define STORE_SETTINGS_ADDR      0x0800F800UL
#define STORE_LOG_ADDR           0x0800FC00UL
#define STORE_PAGE_SIZE          1024UL

#endif  // CONFIG_H
