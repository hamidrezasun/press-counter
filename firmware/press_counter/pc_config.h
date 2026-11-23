// pc_config.h - pin map and tunables of the press counter (v6 board: one ESP32-WROOM-32 does everything).
// The nets are named in hardware/tools/design.py (ESP_PINS).
#ifndef PC_CONFIG_H
#define PC_CONFIG_H

#include <Arduino.h>
#include "pc_proto.h"

// ── pins (ESP32 GPIO numbers) ────────────────────────────────────────────────────────────────────
#if defined(CONFIG_IDF_TARGET_ESP32S3)
// Bench-test pin map for a bare ESP32-S3 devkit (16 MB flash, 8 MB PSRAM).  NOT the v6 board:
// there is no relay driver, no opto input, no RC522, no OLED and no DS18B20 on a devkit, so expect
// E01 / E02 / E13 and use it to exercise WiFi setup, the web page, MQTT and the console only.
// S3 rules: ADC1 = GPIO1..GPIO10, GPIO26..37 are flash / PSRAM, GPIO19/20 are native USB.
// Avoided: 0 (BOOT), 3/45/46 (strapping), 19/20 (USB), 26-37 (flash/PSRAM), 43/44 (UART0).
#define PIN_PRESS        4      // digital input, internal pull-up enabled in press::begin() on S3
#define PIN_RELAY        5      // HIGH = machine enabled (no driver on a devkit: just a GPIO)
#define PIN_BUZZER       6      // HIGH = on
#define PIN_LED_ALERT    7      // red LED
#define PIN_OUT1         15     // OUT1..OUT4
#define PIN_OUT2         16
#define PIN_OUT3         17
#define PIN_OUT4         18
#define PIN_V12          1      // ADC1_CH0 (S3: GPIO1..10 only)
#define PIN_AUX          2      // ADC1_CH1
#define PIN_BTN_SUP      8      // CALL SUPERVISOR, active LOW
#define PIN_BTN_NET      9      // CALL NET, active LOW
#define PIN_SPI_SCK      12     // RC522
#define PIN_SPI_MISO     13
#define PIN_SPI_MOSI     11
#define PIN_RC_SS        10
#define PIN_RC_RST       14
#define PIN_I2C_SDA      21     // OLED
#define PIN_I2C_SCL      47
#define PIN_TEMP         38     // DS18B20 data
// Console: native USB CDC (build with CDCOnBoot=cdc); UART0 is not wired to the USB port on this devkit.
#else
#define PIN_PRESS        34     // PC817 output, 22k pull-up on the board, LOW = press voltage present (input-only pin)
#define PIN_RELAY        26     // -> 1k -> SS8050 -> relay K1 coil, HIGH = machine enabled; 10k pull-down keeps it off while booting
#define PIN_BUZZER       14     // -> 1k -> SS8050 -> 5 V active buzzer, HIGH = on (IO14 toggles at boot: a short chirp is harmless)
#define PIN_LED_ALERT    15     // red LED (470R), HIGH = on.   The green "machine on" LED sits on the relay coil (hardware).
#define PIN_OUT1         27     // output relays K2..K5 (OUT1..OUT4): -> 1k -> SS8050, HIGH = on, 10k pull-down while booting
#define PIN_OUT2         16
#define PIN_OUT3         25
#define PIN_OUT4         13
#define PIN_V12          36     // ADC1_CH0 (SENSOR_VP): 12/24 V sense input J7, divider 100k / 10k (0-30 V)
#define PIN_AUX          35     // ADC1_CH7: AUX input J9, divider 100k / 10k (0-30 V)
#define PIN_BTN_SUP      32     // CALL SUPERVISOR, active LOW (10k pull-up + 100nF on the board)
#define PIN_BTN_NET      33     // CALL NET / MAINTENANCE, active LOW
#define PIN_SPI_SCK      18     // VSPI: RC522
#define PIN_SPI_MISO     19
#define PIN_SPI_MOSI     23
#define PIN_RC_SS        5      // RC522 SDA/SS (10k pull-up: also the IO5 boot strap)
#define PIN_RC_RST       17
#define PIN_I2C_SDA      21     // OLED (2.2k pull-ups on the board)
#define PIN_I2C_SCL      22
#define PIN_TEMP         4      // DS18B20 data (2.2k pull-up, 100R series)
// UART0 (GPIO1 / GPIO3) = USB console through the CH340G; IO0 / EN are driven by its DTR / RTS for programming.
// Unused: IO2, IO12 (boot straps, left open on purpose), IO39 (input only).
#endif

#define CONSOLE_BAUD     115200UL

// ── display: 0.96in 128x64 SSD1306 I2C module ─────────────────────────────────────────────────────
#define OLED_ADDR        0x3C   // most modules; some are 0x3D (the firmware tries both)
#define OLED_I2C_HZ      400000UL

// ── timing / behaviour ───────────────────────────────────────────────────────────────────────────
#define BTN_DEBOUNCE_MS          25UL
#define BTN_CANCEL_CALL_MS       1500UL        // hold a call button this long to cancel the call
#define BTN_SETUP_MS             6000UL        // hold BOTH call buttons this long: open the WiFi setup access point
#define TAG_COOLDOWN_MS          3000UL        // ignore the same card for 3 s after it was handled
#define TAG_WRITE_TIMEOUT_MS     30000UL
#define NFC_POLL_MS              150UL
#define NFC_HEALTH_MS            5000UL
#define TEMP_SAMPLE_MS           2000UL        // DS18B20: one conversion (750 ms) every 2 s
#define TEMP_REPORT_MS           60000UL
#define PRESS_GAP_MS             3UL           // an inactive gap shorter than this inside a pulse is contact bounce
#define PRESS_TRAIN_GAP_MS       1000UL
#define SERVER_ERROR_MS          300000UL      // no MQTT for 5 minutes -> E06 (warning)
#define EVENT_RETRY_MS           2500UL
#define WDT_TIMEOUT_S            15
#define UI_REFRESH_MS            250UL
#define DISPLAY_DIM_MS           120000UL      // dim the OLED after 2 minutes without a stroke / card / button
#define LIFETIME_SAVE_EVERY      25UL
#define STATUS_PERIOD_MS         5000UL        // MQTT status
#define INFO_PERIOD_MS           600000UL      // MQTT device info
#define MQTT_RETRY_MS            10000UL
#define LOG_LINES                32
#define LOG_LINE_LEN             100

#define V12_DIVIDER              11.0f         // (100k + 10k) / 10k
#define AUX_DIVIDER              11.0f         // (100k + 10k) / 10k
#define SENSE_SAMPLE_MS          10UL          // DC inputs: one reading of each every 10 ms ...
#define SENSE_AVERAGE            20            // ... averaged over 20 readings (200 ms = 10 mains periods)
#define VOLTAGE_REPORT_MS        5000UL

#define MAX_TAG_TEXT             38            // fits the three data blocks of one MIFARE Classic sector
#define EVENT_QUEUE_SIZE         64
#define NAME_LEN                 24

#endif  // PC_CONFIG_H
