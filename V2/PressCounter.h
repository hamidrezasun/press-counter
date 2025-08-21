#ifndef PRESS_COUNTER_H
#define PRESS_COUNTER_H

#include <Arduino.h>
#include "InfluxDB.h"

class PressCounter {
public:
  PressCounter(uint8_t pin, InfluxDB& influx);
  void begin();
  void handle();
  void reset();
  unsigned long getCount() const;

private:
  uint8_t counterPin;
  InfluxDB& influxDB;
  unsigned long count;
  bool lastState;
  unsigned long lastDebounceTime;
};

#endif