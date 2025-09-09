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
  volatile unsigned long pulseStart;
  volatile unsigned long lastPulseDuration;
  volatile unsigned long lastPulseTime;
  volatile unsigned int pulseCount;
  volatile bool inPulseTrain;
  volatile unsigned long pulseTrainStart;
  unsigned long minPulseDuration;
  
  static void ICACHE_RAM_ATTR handlePulse();
  static PressCounter* instance;
};

#endif