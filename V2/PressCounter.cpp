#include "PressCounter.h"

PressCounter::PressCounter(uint8_t pin, InfluxDB& influx)
  : counterPin(pin), influxDB(influx), count(0), lastState(false), lastDebounceTime(0) {}

void PressCounter::begin() {
  pinMode(counterPin, INPUT_PULLUP);
}

void PressCounter::handle() {
  bool reading = digitalRead(counterPin);
  if (reading != lastState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > 50) { // 50ms debounce
    if (reading != lastState && !reading) { // Falling edge
      count++;
      char fields[32];
      snprintf(fields, sizeof(fields), "count=%lu", count);
      influxDB.sendData("press_counter", fields);
    }
    lastState = reading;
  }
}

void PressCounter::reset() {
  count = 0;
}

unsigned long PressCounter::getCount() const {
  return count;
}