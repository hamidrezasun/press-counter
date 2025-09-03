#include <Arduino.h>
#include "ConfigManager.h"
#include "WebServerManager.h"
#include "InfluxDB.h"
#include "ControlManager.h"
#include "NfcManager.h"
#include "PressCounter.h"

ESP8266WebServer server(80);
WiFiClient wifiClient;

ConfigManager configManager;
ControlManager controlManager(configManager);
NfcManager nfcManager;
WebServerManager webManager(server, configManager, controlManager, nfcManager);
InfluxDB influxDB(configManager);
PressCounter pressCounter(PRESS_SENSOR_PIN,influxDB);

void setup() {
  Serial.begin(74880);
  Serial.println("---startup begin---");
  delay(1000);
  controlManager.begin();
  controlManager.playStartupTone();

  pressCounter.begin();
  configManager.load();
  nfcManager.begin();
  webManager.begin();
  influxDB.begin();
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);

  // Wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  // Print assigned IP
  if (WiFi.status() == WL_CONNECTED) {
    influxDB.sendDeviceInfo();
  } else {
    controlManager.playErrorSound();
  }
}

void loop() {
  webManager.handleClient();
  controlManager.handleButton();
  controlManager.buzzerLoop();
  pressCounter.handle();
  
  // Handle NFC read/write
  if (nfcManager.handleNfcLoop()) {
    // Write successful
    webManager.updateStatus(); // Update UI to reflect written data
    controlManager.playSuccessSound();
  } else if (strlen(nfcManager.getLastTagData()) > 0) {
    // Tag read, update UI
    webManager.updateStatus();
  }
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    webManager.updateStatus();
    lastUpdate = millis();
  }
}