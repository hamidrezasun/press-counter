#include <Arduino.h>
#include "ConfigManager.h"
#include "WiFiManagerSetup.h"
#include "WebServerManager.h"
#include "InfluxDB.h"
#include "ControlManager.h"
#include "NfcManager.h"
#include "PressCounter.h"

AsyncWebServer server(80);
DNSServer dns;
WiFiClient wifiClient;

ConfigManager configManager;
WiFiManagerSetup wifiSetup(server, dns);
ControlManager controlManager(configManager);
NfcManager nfcManager;
WebServerManager webManager(server, configManager, controlManager, nfcManager);
InfluxDB influxDB(configManager);
PressCounter pressCounter(PRESS_SENSOR_PIN,influxDB);

void setup() {
  Serial.begin(74880);
  delay(1000);

  controlManager.begin();
  controlManager.playStartupTone();

  pressCounter.begin();
  configManager.load();      
  wifiSetup.begin();
  nfcManager.begin();
  webManager.begin();
  influxDB.begin();

  if (WiFi.status() == WL_CONNECTED) {
    influxDB.sendDeviceInfo();
  } else {
    controlManager.playErrorSound();
  }
}

void loop() {
  controlManager.handleButton();
  controlManager.buzzerLoop();
  pressCounter.handle();
  
  // Handle NFC read/write
  if (nfcManager.handleNfcLoop()) {
    // Write successful
    webManager.updateStatus(); // Update UI to reflect written data
    controlManager.playSuccessSound();
  } else if (!nfcManager.getLastTagData().isEmpty()) {
    // Tag read, update UI
    webManager.updateStatus();
  }
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    webManager.updateStatus();
    lastUpdate = millis();
  }
  
  delay(10);
}