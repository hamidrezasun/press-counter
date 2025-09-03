#include "PressCounter.h"

PressCounter* PressCounter::instance = nullptr;

PressCounter::PressCounter(uint8_t pin, InfluxDB& influx)
  : counterPin(pin), influxDB(influx), pulseStart(0), lastPulseDuration(0),
    lastPulseTime(0), pulseCount(0), inPulseTrain(false), pulseTrainStart(0),
    minPulseDuration(10) {} // Default minimum pulse duration: 10ms

void ICACHE_RAM_ATTR PressCounter::handlePulse() {
  if (PressCounter::instance) {
    int pinState = digitalRead(PressCounter::instance->counterPin);
    unsigned long currentMillis = millis();
    
    if (pinState == LOW) { // Active LOW (button pressed)
      // Pulse start (falling edge)
      if (PressCounter::instance->pulseStart == 0) {
        PressCounter::instance->pulseStart = currentMillis;
        if (!PressCounter::instance->inPulseTrain) {
          PressCounter::instance->inPulseTrain = true;
          PressCounter::instance->pulseTrainStart = currentMillis;
        }
      }
    } else {
      // Pulse end (rising edge)
      if (PressCounter::instance->pulseStart > 0) {
        unsigned long duration = currentMillis - PressCounter::instance->pulseStart;
        
        // Only count pulses longer than the minimum duration
        if (duration >= PressCounter::instance->minPulseDuration) {
          PressCounter::instance->lastPulseDuration = duration;
          PressCounter::instance->lastPulseTime = currentMillis;
          PressCounter::instance->pulseCount++;
          
          // Send to InfluxDB
          char fields[32];
          snprintf(fields, sizeof(fields), "count=%lu", PressCounter::instance->pulseCount);
          PressCounter::instance->influxDB.sendData("press_counter", fields);
        }
        PressCounter::instance->pulseStart = 0;
      }
    }
  }
}

void PressCounter::begin() {
  instance = this;
  pinMode(counterPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(counterPin), handlePulse, CHANGE);
  Serial.println("PressCounter initialized with interrupt");
}

void PressCounter::handle() {
  // Check if pulse train has ended (no activity for 1 second)
  if (inPulseTrain && (millis() - lastPulseTime > 1000)) {
    inPulseTrain = false;
    pulseTrainStart = 0;
  }
}

void PressCounter::reset() {
  noInterrupts();
  pulseCount = 0;
  lastPulseTime = 0;
  lastPulseDuration = 0;
  inPulseTrain = false;
  pulseTrainStart = 0;
  pulseStart = 0;
  interrupts();
  Serial.println("PressCounter reset");
}

unsigned long PressCounter::getCount() const {
  return pulseCount;
}

unsigned long PressCounter::getLastPulseDuration() const {
  return lastPulseDuration;
}

unsigned long PressCounter::getLastPulseTime() const {
  return lastPulseTime;
}

bool PressCounter::getInPulseTrain() const {
  return inPulseTrain;
}

unsigned long PressCounter::getPulseTrainStart() const {
  return pulseTrainStart;
}

void PressCounter::setMinPulseDuration(unsigned long minDuration) {
  minPulseDuration = minDuration;
}