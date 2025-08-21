#ifndef PRESSCOUNTER_H
#define PRESSCOUNTER_H

#include <ArduinoJson.h>
#include "Config.h"

class Network; // Forward declaration

class PressCounter {
private:
  int opto_pin;
  int debounce;
  bool count_enabled = false;
  int press_count = 0;
  unsigned long last_debounce_time = 0;
  bool last_state = HIGH;
  Network* network;
  Config* config;

public:
  void setupPress(int pin, int deb, Network& net, Config& conf);
  void loopPress();
  void startCounting();
  void stopCounting();
};

#endif