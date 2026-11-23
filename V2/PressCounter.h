// PressCounter - counts press cycles from the opto-isolated 230 V sense input.
// The PC817 output is LOW while the sensed voltage is present; one LOW period >= minPulseDuration = one press.
#ifndef PRESS_COUNTER_H
#define PRESS_COUNTER_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "ControlManager.h"
#include "InfluxDB.h"

class PressCounter {
public:
  PressCounter(InfluxDB& influx, ConfigManager& cfg, ControlManager& ctrl);

  void begin();
  void handle();                       // call every loop()
  void reset();                        // new session: counter back to zero
  void updateMinPulseDuration();

  unsigned long getCount() const { return _count; }
  unsigned long getLastPulseDuration() const { return _lastDuration; }
  unsigned long getLastPulseTime() const { return _lastPulseTime; }      // millis() of the last counted press end, 0 = none
  bool getInPulseTrain() const { return _lastPulseTime && millis() - _lastPulseTime <= PRESS_TRAIN_GAP_MS; }

private:
  InfluxDB&       _influx;
  ConfigManager&  _config;
  ControlManager& _ctrl;

  bool _active = false;                // opto currently LOW (after debounce)
  bool _lastRaw = false;
  unsigned long _rawChanged = 0;
  unsigned long _activeSince = 0;
  unsigned long _minPulse = 10;
  unsigned long _count = 0, _lastDuration = 0, _lastPulseTime = 0;
};

#endif
