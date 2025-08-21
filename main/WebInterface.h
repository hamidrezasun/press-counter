#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include <ESP8266WebServer.h>
#include "Config.h"

class WebInterface {
private:
  ESP8266WebServer server; // Changed to direct initialization
  Config* config;

public:
  WebInterface();
  void setupWeb(Config& conf);
  void loop();
};

#endif