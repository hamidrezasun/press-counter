// define.h
#ifndef DEFINE_H
#define DEFINE_H

#define WIFI_SSID "TW factory"
#define WIFI_PASSWORD  "e8cac589tw"
#define BUZZER_PIN      15   // D8 — OK as output; see note below
#define RELAY_PIN        5   // D1 — stable at boot; avoids startup pulse
#define BUTTON_PIN      16   // D0 — input OK (no interrupts on GPIO16)
#define CS_PIN           4   // D2 — MFRC522 CS (works fine with HSPI)
#define MAX7219           0   // D3 — MFRC522 RST (active-low, keep HIGH at boot)
#define PRESS_SENSOR_PIN 2   // D4 — per your requirement (must not be LOW at boot)
#define DEBOUNCE_DELAY 50   // Default debounce delay in ms
#define TEMP_SENSOR_PIN A0
#endif