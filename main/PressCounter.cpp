#include "HardwareSerial.h"
#include "PressCounter.h"
#include "Network.h"
#include "RFIDHandler.h"

extern String current_die; // Declared in RFIDHandler.cpp

void PressCounter::setupPress(int pin, int deb, Network& net, Config& conf) {
  opto_pin = pin;
  debounce = deb;
  network = &net;
  config = &conf;
  pinMode(opto_pin, INPUT_PULLUP);
}

void PressCounter::loopPress() {
  bool reading = digitalRead(opto_pin);
  if (reading != last_state) {
    last_debounce_time = millis();
  }

  if ((millis() - last_debounce_time) > debounce) {
    if (reading == LOW && last_state == HIGH) { // Becoming low on press down
      if (count_enabled) {
        Serial.println("Count signal sent");
        press_count++;
        network->sendCountSignal(press_count);
      }
    }
    last_state = reading;
  }
}

void PressCounter::startCounting() {
  count_enabled = true;
  press_count = 0;
  network->sendDeviceInfo();
  config->setCounting(true); // Save counting state
}

void PressCounter::stopCounting() {
  count_enabled = false;
  press_count = 0;
  config->setCounting(false); // Save counting state
}