#include "PressCounter.h"
#include "ConfigManager.h"

PressCounter* PressCounter::instance = nullptr;

PressCounter::PressCounter(uint8_t pin, InfluxDB& influx, ConfigManager& cfg, ControlManager& ctrl)
  : counterPin(pin), influxDB(influx), configManager(cfg), controlManager(ctrl), pulseStart(0), 
    lastPulseDuration(0), lastPulseTime(0), pulseCount(0), inPulseTrain(false), 
    pulseTrainStart(0) {
  minPulseDuration = configManager.get().minPulseDuration;
}

void ICACHE_RAM_ATTR PressCounter::handlePulse() {
  if (PressCounter::instance) {
    int pinState = digitalRead(PressCounter::instance->counterPin);
    unsigned long currentMillis = millis();
    
    if (pinState == LOW) {
      if (PressCounter::instance->pulseStart == 0) {
        PressCounter::instance->pulseStart = currentMillis;
        if (!PressCounter::instance->inPulseTrain) {
          PressCounter::instance->inPulseTrain = true;
          PressCounter::instance->pulseTrainStart = currentMillis;
        }
      }
    } else {
      if (PressCounter::instance->pulseStart > 0) {
        unsigned long duration = currentMillis - PressCounter::instance->pulseStart;
        
        if (duration >= PressCounter::instance->minPulseDuration) {
          PressCounter::instance->lastPulseDuration = duration;
          PressCounter::instance->lastPulseTime = currentMillis;
          PressCounter::instance->pulseCount++;
          
          char fields[32];
          snprintf(fields, sizeof(fields), "count=%ui", PressCounter::instance->pulseCount);
          PressCounter::instance->influxDB.sendData("press_counter", fields);
          
          PressCounter::instance->controlManager.playSuccessSound();
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
  
  minPulseDuration = configManager.get().minPulseDuration;
  
  Serial.println("PressCounter initialized with interrupt");
  Serial.print("Min pulse duration: ");
  Serial.print(minPulseDuration);
  Serial.println("ms");
}

void PressCounter::handle() {
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

void PressCounter::updateMinPulseDuration() {
  minPulseDuration = configManager.get().minPulseDuration;
  Serial.print("Updated min pulse duration: ");
  Serial.print(minPulseDuration);
  Serial.println("ms");
}