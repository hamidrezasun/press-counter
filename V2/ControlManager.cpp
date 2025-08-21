#include "ControlManager.h"

ControlManager::ControlManager(ConfigManager& cfg)
  : configManager(cfg), relayState(false), buzzerState(false), buttonState(false),
    lastDebounceTime(0), lastButtonReading(true), buzzerMode(IDLE), buzzerStartTime(0),
    lastBuzzerToggle(0), buzzerStep(0) {}

void ControlManager::begin() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay off initially
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button with internal pull-up
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Buzzer off initially
}

void ControlManager::handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > configManager.get().debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
    }
  }
  lastButtonReading = reading;
}

DeviceStatus ControlManager::getStatus() {
  return {relayState, buzzerState, buttonState};
}

void ControlManager::setRelayState(bool state, bool toggle) {
  if (toggle) {
    relayState = !relayState;
  } else {
    relayState = state;
  }
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
}

void ControlManager::startBuzzer(BuzzerMode mode) {
  buzzerMode = mode;
  buzzerStartTime = millis();
  lastBuzzerToggle = millis();
  buzzerStep = 0;
  buzzerState = false;
  digitalWrite(BUZZER_PIN, LOW);
}

void ControlManager::playErrorSound() {
  startBuzzer(ERROR);
}

void ControlManager::playWarningSound() {
  startBuzzer(WARNING);
}

void ControlManager::playSuccessSound() {
  startBuzzer(SUCCESS);
}

void ControlManager::playStartupTone() {
  startBuzzer(STARTUP);
}

void ControlManager::buzzerLoop() {
  if (buzzerMode == IDLE) {
    if (buzzerState) {
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
    return;
  }

  unsigned long currentTime = millis();

  if (buzzerMode == BLINKING) {
    if (currentTime - lastBuzzerToggle >= 500) { // 500ms blink interval
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      lastBuzzerToggle = currentTime;
    }
    return;
  }

  if (buzzerMode == ERROR) {
    if (buzzerStep < 3) {
      if (buzzerStep % 2 == 0) { // Beep on
        if (currentTime - lastBuzzerToggle < 200) return; // 200ms beep
        buzzerState = false;
        digitalWrite(BUZZER_PIN, LOW);
        lastBuzzerToggle = currentTime;
        buzzerStep++;
      } else { // Pause
        if (currentTime - lastBuzzerToggle < 100) return; // 100ms pause
        buzzerState = true;
        digitalWrite(BUZZER_PIN, HIGH);
        lastBuzzerToggle = currentTime;
        buzzerStep++;
      }
    } else {
      buzzerMode = IDLE;
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else if (buzzerMode == WARNING) {
    if (buzzerStep == 0) {
      if (currentTime - lastBuzzerToggle < 500) return; // 500ms beep
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      buzzerStep++;
    } else {
      buzzerMode = IDLE;
    }
  } else if (buzzerMode == SUCCESS) {
    if (buzzerStep == 0) {
      if (currentTime - lastBuzzerToggle < 100) return; // 100ms beep
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      buzzerStep++;
    } else {
      buzzerMode = IDLE;
    }
  } else if (buzzerMode == STARTUP) {
    if (buzzerStep < 2) {
      if (buzzerStep % 2 == 0) { // Beep on
        if (currentTime - lastBuzzerToggle < 100) return; // 100ms beep
        buzzerState = false;
        digitalWrite(BUZZER_PIN, LOW);
        lastBuzzerToggle = currentTime;
        buzzerStep++;
      } else { // Pause
        if (currentTime - lastBuzzerToggle < 100) return; // 100ms pause
        buzzerState = true;
        digitalWrite(BUZZER_PIN, HIGH);
        lastBuzzerToggle = currentTime;
        buzzerStep++;
      }
    } else {
      buzzerMode = IDLE;
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}