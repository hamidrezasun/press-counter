#ifndef PRESS_COUNTER_H
#define PRESS_COUNTER_H

#include <Arduino.h>
#include "InfluxDB.h"
#include "ConfigManager.h"
#include "ControlManager.h"

class PressCounter {
public:
  PressCounter(uint8_t pin, InfluxDB& influx, ConfigManager& cfg, ControlManager& ctrl);
  void begin();
  void handle();
  void reset();
  unsigned long getCount() const;
  unsigned long getLastPulseDuration() const;
  unsigned long getLastPulseTime() const;
  bool getInPulseTrain() const;
  unsigned long getPulseTrainStart() const;
  void updateMinPulseDuration();

private:
  uint8_t counterPin;
  InfluxDB& influxDB;
  ConfigManager& configManager;
  ControlManager& controlManager;
  unsigned long pulseStart;
  unsigned long lastPulseDuration;
  unsigned long lastPulseTime;
  unsigned int pulseCount;
  bool inPulseTrain;
  unsigned long pulseTrainStart;
  unsigned long minPulseDuration;
};

#endif