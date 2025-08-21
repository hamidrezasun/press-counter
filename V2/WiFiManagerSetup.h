#ifndef WIFI_MANAGER_SETUP_H
#define WIFI_MANAGER_SETUP_H

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

class WiFiManagerSetup {
public:
  WiFiManagerSetup(AsyncWebServer& server, DNSServer& dns);
  void begin();

private:
  AsyncWebServer& server;
  DNSServer& dns;
};

#endif