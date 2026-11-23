// ControlManager - relay, user button, buzzer and LM35 temperature (everything that touches a pin except RFID / press input)
#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "InfluxDB.h"
#include "define.h"

struct DeviceStatus {
  bool relayOn;
  bool buzzerOn;
  bool buttonPressed;
};

class ControlManager {
public:
  ControlManager(ConfigManager& cfg, InfluxDB& influx);

  void begin();
  void handle();                       // call from loop(): button, buzzer pattern, periodic temperature report

  // relay
  void setRelay(bool on, const char* reason);   // logs device_status to InfluxDB on every real change
  void toggleRelay(const char* reason);
  bool relayOn() const { return _relayOn; }
  unsigned long relayOnSince() const { return _relayOnSince; }

  // buzzer (active buzzer: on/off patterns, non-blocking)
  void playSuccessSound();
  void playWarningSound();
  void playErrorSound();
  void playStartupTone();

  float readTemperature();             // degrees C (LM35, 10 mV/degC)
  DeviceStatus getStatus() const;

private:
  ConfigManager& _config;
  InfluxDB&      _influx;

  bool _relayOn = false;
  unsigned long _relayOnSince = 0;

  // button
  bool _btnStable = HIGH, _btnLast = HIGH;
  unsigned long _btnChanged = 0;

  // buzzer pattern: alternating on/off durations in ms, starting with ON
  const uint16_t* _pattern = nullptr;
  uint8_t _patternLen = 0, _patternPos = 0;
  unsigned long _patternAt = 0;
  bool _buzzerOn = false;
  void startPattern(const uint16_t* p, uint8_t n);
  void buzzer(bool on);

  unsigned long _lastTempReport = 0;
  void reportTemperature();
};

#endif
