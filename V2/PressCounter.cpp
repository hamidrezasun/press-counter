#include "PressCounter.h"
#include "ConfigManager.h"

PressCounter::PressCounter(uint8_t pin, InfluxDB& influx, ConfigManager& cfg, ControlManager& ctrl)
  : counterPin(pin), influxDB(influx), configManager(cfg), controlManager(ctrl), pulseStart(0), 
    lastPulseDuration(0), lastPulseTime(0), pulseCount(0), inPulseTrain(false), 
    pulseTrainStart(0) {
  minPulseDuration = configManager.get().minPulseDuration;
}

void PressCounter::begin() {
  pinMode(counterPin, INPUT_PULLUP);
  
  minPulseDuration = configManager.get().minPulseDuration;
  
  Serial.println("PressCounter initialized with polling");
  Serial.print("Min pulse duration: ");
  Serial.print(minPulseDuration);
  Serial.println("ms");
}

void PressCounter::handle() {
  int pinState = digitalRead(counterPin);
  unsigned long currentMillis = millis();
  
  if (pinState == LOW) {
    if (pulseStart == 0) {
      pulseStart = currentMillis;
      if (!inPulseTrain) {
        inPulseTrain = true;
        pulseTrainStart = currentMillis;
      }
    }
  } else {
    if (pulseStart > 0) {
      unsigned long duration = currentMillis - pulseStart;
      
      if (duration >= minPulseDuration) {
        lastPulseDuration = duration;
        lastPulseTime = currentMillis;
        pulseCount++;
        
        // Send pulse count, pulse duration, and temperature to InfluxDB
        float temperature = controlManager.readTemperature();
        char fields[64];
        snprintf(fields, sizeof(fields), "count=%u,duration=%lu,temperature=%.2f", 
                 pulseCount, lastPulseDuration, temperature);
        influxDB.sendData("press_counter", fields);        
        controlManager.playSuccessSound(300);
      }
      pulseStart = 0;
    }
  }
  
  if (inPulseTrain && (millis() - lastPulseTime > 1000)) {
    inPulseTrain = false;
    pulseTrainStart = 0;
  }
}

void PressCounter::reset() {
  pulseCount = 0;
  lastPulseTime = 0;
  lastPulseDuration = 0;
  inPulseTrain = false;
  pulseTrainStart = 0;
  pulseStart = 0;
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