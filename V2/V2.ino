// Hydraulic press counter - ESP-12E/F firmware.   Hardware: hardware/press_counter (KiCad project).
//
// Libraries: ESP8266 core, MFRC522, NDEF (NfcAdapter for MFRC522), ArduinoJson 6, ESPAsyncTCP + asyncHTTPrequest.
// WiFi credentials: copy secrets.h.example to secrets.h.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "define.h"
#include "ConfigManager.h"
#include "InfluxDB.h"
#include "ControlManager.h"
#include "PressCounter.h"
#include "NfcManager.h"
#include "WebServerManager.h"

// Object graph (declaration order matters: later objects reference earlier ones)
static ESP8266WebServer server(80);
static ConfigManager    configManager;
static InfluxDB         influxDB(configManager);
static ControlManager   controlManager(configManager, influxDB);
static PressCounter     pressCounter(influxDB, configManager, controlManager);
static NfcManager       nfcManager;
static WebServerManager webManager(server, configManager, controlManager, nfcManager, pressCounter);

static void connectWifi() {
  if (!WIFI_SSID[0]) {
    Serial.println("[WiFi] no SSID configured - create secrets.h (see secrets.h.example). Running offline.");
    controlManager.playWarningSound();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);                 // the core re-joins by itself after a router reboot
  WiFi.persistent(false);
  WiFi.hostname(configManager.get().device);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WiFi] connecting");
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(250);
    Serial.print('.');
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] connected, IP %s\n", WiFi.localIP().toString().c_str());
    influxDB.sendDeviceInfo();
    controlManager.playSuccessSound();
  } else {
    Serial.println("\n[WiFi] not connected yet - continuing offline, will keep retrying");
    controlManager.playErrorSound();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- press counter boot ---");

  controlManager.begin();                      // relay OFF first
  configManager.load();
  influxDB.begin();
  pressCounter.begin();

  if (!nfcManager.begin()) {
    Serial.println("[NFC] RC522 not detected - check the module in socket P1");
    controlManager.playErrorSound();
  }
  webManager.begin();
  connectWifi();
  controlManager.playStartupTone();
}

void loop() {
  webManager.handleClient();
  controlManager.handle();
  pressCounter.handle();
  nfcManager.handle(controlManager, influxDB, pressCounter);
  influxDB.loop();

  // Safety: switch the machine off after 10 min without a press (counted from relay-on or the last press)
  if (controlManager.relayOn()) {
    const unsigned long lastActivity = max(controlManager.relayOnSince(), pressCounter.getLastPulseTime());
    if (millis() - lastActivity > RELAY_IDLE_TIMEOUT_MS) controlManager.setRelay(false, "idle timeout");
  }
}
