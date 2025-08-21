#include "WiFiManagerSetup.h"

WiFiManagerSetup::WiFiManagerSetup(AsyncWebServer& srv, DNSServer& dnsSrv)
  : server(srv), dns(dnsSrv) {}

void WiFiManagerSetup::begin() {
  AsyncWiFiManager wifiManager(&server, &dns);

  if (!wifiManager.autoConnect("ESP_ConfigAP")) {
    Serial.println("Failed to connect, restarting...");
    ESP.restart();
  }

  Serial.println("WiFi connected!");
  Serial.println("IP address: " + WiFi.localIP().toString());
}