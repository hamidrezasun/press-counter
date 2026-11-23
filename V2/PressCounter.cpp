#include "PressCounter.h"

#define GLITCH_MS 2UL      // ignore sub-2ms spikes on the sense line

PressCounter::PressCounter(InfluxDB& influx, ConfigManager& cfg, ControlManager& ctrl)
  : _influx(influx), _config(cfg), _ctrl(ctrl) {}

void PressCounter::begin() {
  pinMode(PRESS_SENSOR_PIN, INPUT);             // external 10k pull-up; GPIO16 has no internal pull-up
  _lastRaw = _active = (digitalRead(PRESS_SENSOR_PIN) == LOW);
  updateMinPulseDuration();
}

void PressCounter::updateMinPulseDuration() {
  int v = _config.get().minPulseDuration;
  if (v < 1) v = 1;
  _minPulse = (unsigned long)v;
  Serial.printf("[Press] min pulse %lu ms\n", _minPulse);
}

void PressCounter::handle() {
  const unsigned long now = millis();
  const bool raw = (digitalRead(PRESS_SENSOR_PIN) == LOW);
  if (raw != _lastRaw) { _lastRaw = raw; _rawChanged = now; }
  if (raw == _active || now - _rawChanged < GLITCH_MS) return;

  _active = raw;
  if (_active) {                                // rising edge of the press signal
    _activeSince = _rawChanged;
    return;
  }
  const unsigned long duration = _rawChanged - _activeSince;
  if (duration < _minPulse) return;             // too short: noise

  _count++;
  _lastDuration = duration;
  _lastPulseTime = now ? now : 1;               // 0 means "never"
  char fields[96];
  // plain numbers (not "...i"): the existing InfluxDB bucket stores these fields as floats
  snprintf(fields, sizeof(fields), "count=%lu,duration=%lu,temperature=%.2f",
           _count, duration, _ctrl.readTemperature());
  _influx.sendData("press_counter", fields);
  _ctrl.playSuccessSound();
}

void PressCounter::reset() {
  _count = 0;
  _lastDuration = 0;
  _lastPulseTime = 0;
  Serial.println("[Press] counter reset");
}
