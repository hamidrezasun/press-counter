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
InfluxDB influxDB(configManager);
PressCounter pressCounter(PRESS_SENSOR_PIN, influxDB, configManager, controlManager);
WebServerManager webManager(server, configManager, controlManager, nfcManager, pressCounter);

unsigned long lastPressTime = 0;
const unsigned long PRESS_TIMEOUT = 10 * 60 * 1000; // 10 minutes in milliseconds

const unsigned long nfcINTERVAL = 2000;
unsigned long nfcPreviousMillis = 0;

void setup() {
  Serial.begin(74880);
  Serial.println("---startup begin---");
  delay(1000);
  controlManager.begin();
  //controlManager.playStartupTone();
  configManager.load();
  pressCounter.begin();
  if(!nfcManager.begin()){
    controlManager.playErrorSound();
    Serial.println("NFC not connected!");
  }
  webManager.begin();
  influxDB.begin();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD );

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  if (WiFi.status() == WL_CONNECTED) {
    influxDB.sendDeviceInfo();
    controlManager.playSuccessSound();
  } else {
    controlManager.playErrorSound();
  }
  controlManager.startTemperatureReporting();
}

void loop() {
    webManager.handleClient();
    controlManager.handleButton();
    pressCounter.handle();
    
    // Check if press counter has been idle for 10 minutes
    if (pressCounter.getLastPulseTime() > 0 && 
        millis() - pressCounter.getLastPulseTime() > PRESS_TIMEOUT &&
        controlManager.getStatus().relayOn) {
        controlManager.setRelayState(false, false, &influxDB);
        Serial.println("Turned off relay due to 10-minute inactivity");
    }
    
    unsigned long nfcCurrentMillis = millis();
    if (nfcCurrentMillis - nfcPreviousMillis >= nfcINTERVAL) {
      nfcPreviousMillis = nfcCurrentMillis;
      nfcManager.handleNfcLoop(webManager, controlManager, influxDB, pressCounter);  // Add pressCounter parameter
    }
}