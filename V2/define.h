// define.h - pin map and tunables.  Pin numbers match hardware/press_counter (schematic v2).
#ifndef DEFINE_H
#define DEFINE_H

#include <Arduino.h>

// ── WiFi credentials ─────────────────────────────────────────────────────────
// Put them in secrets.h (copy secrets.h.example, git-ignored) - never commit them.
#if defined(__has_include)
  #if __has_include("secrets.h")
    #include "secrets.h"
  #endif
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID     ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif

// ── Pin map (ESP-12E/F) ──────────────────────────────────────────────────────
#define PRESS_SENSOR_PIN  16   // PC817 opto output, external 10k pull-up, LOW = press voltage present
#define BUTTON_PIN         2   // user button to GND, external 10k pull-up (GPIO2 must be HIGH at boot)
#define RELAY_PIN          5   // -> 1k -> BC337 -> relay module IN (HIGH = relay ON)
#define BUZZER_PIN        15   // -> BC337 -> active buzzer (GPIO15 must be LOW at boot: 10k pull-down)
#define NFC_SS_PIN         4   // RC522 SDA/SS (SCK=14, MOSI=13, MISO=12 are the hardware SPI pins)
#define TEMP_SENSOR_PIN   A0   // LM35 via 1k + 100nF
// GPIO0 = FLASH button (do not use), GPIO1/3 = serial

// ── Hardware options ─────────────────────────────────────────────────────────
// The relay module is driven through a BC337 that pulls its (active-LOW) IN pin to GND.
// GPIO HIGH = relay ON. Set to LOW only if you wire the module IN directly to the GPIO.
#ifndef RELAY_ON_LEVEL
  #define RELAY_ON_LEVEL HIGH
#endif
// Full-scale voltage of the ADC input. Bare ESP-12E/F module: 1.0 V.
// NodeMCU/WeMos boards have an on-board divider: use 3.3.
#ifndef ADC_FULL_SCALE_V
  #define ADC_FULL_SCALE_V 1.0f
#endif

// ── Timing / behaviour ───────────────────────────────────────────────────────
#define DEBOUNCE_MS              50UL
#define RELAY_IDLE_TIMEOUT_MS    (10UL * 60UL * 1000UL)   // relay switches off after 10 min without a press
#define NFC_POLL_MS              250UL
#define TAG_COOLDOWN_MS          3000UL                   // ignore the same tag for 3 s after it was handled
#define PENDING_WRITE_TIMEOUT_MS 30000UL                  // a queued tag write expires after 30 s
#define TEMP_REPORT_MS           60000UL
#define PRESS_TRAIN_GAP_MS       1000UL

#define MAX_TAG_DATA_LENGTH      128

#endif // DEFINE_H
