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
  Serial.println("ControlManager initialized");
}

void ControlManager::handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > configManager.get().debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      Serial.println("Button state changed to: " + String(buttonState ? "Pressed" : "Released"));
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
  Serial.println("Relay state set to: " + String(relayState ? "On" : "Off"));
}

void ControlManager::startBuzzer(BuzzerMode mode) {
  if (buzzerMode != IDLE) {
    Serial.println("Buzzer already active, ignoring new mode: " + String(mode));
    return; // Prevent interrupting an ongoing buzzer sequence
  }
  buzzerMode = mode;
  buzzerStartTime = millis();
  lastBuzzerToggle = millis();
  buzzerStep = 0;
  buzzerState = true; // Start with buzzer on for all modes
  digitalWrite(BUZZER_PIN, HIGH); // Activate buzzer immediately
  Serial.println("Starting buzzer mode: " + String(mode));
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
      Serial.println("Buzzer stopped (IDLE)");
    }
    return;
  }

  unsigned long currentTime = millis();

  if (buzzerMode == BLINKING) {
    if (currentTime - lastBuzzerToggle >= 500) { // 500ms blink interval
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      lastBuzzerToggle = currentTime;
      Serial.println("Blinking buzzer toggled: " + String(buzzerState ? "On" : "Off"));
    }
    return;
  }

  if (buzzerMode == ERROR) {
    if (buzzerStep < 6) { // 6 steps: 3 beeps (on-off-on-off-on-off)
      if (buzzerStep % 2 == 0) { // Beep on
        if (currentTime - lastBuzzerToggle >= 200) { // 200ms beep
          buzzerState = false;
          digitalWrite(BUZZER_PIN, LOW);
          lastBuzzerToggle = currentTime;
          buzzerStep++;
          Serial.println("ERROR: Beep off, step " + String(buzzerStep));
        }
      } else { // Pause
        if (currentTime - lastBuzzerToggle >= 100) { // 100ms pause
          buzzerState = true;
          digitalWrite(BUZZER_PIN, HIGH);
          lastBuzzerToggle = currentTime;
          buzzerStep++;
          Serial.println("ERROR: Beep on, step " + String(buzzerStep));
        }
      }
    } else {
      buzzerMode = IDLE;
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("ERROR sequence complete");
    }
  } else if (buzzerMode == WARNING) {
    if (buzzerStep == 0) {
      if (currentTime - lastBuzzerToggle >= 500) { // 500ms beep
        buzzerState = false;
        digitalWrite(BUZZER_PIN, LOW);
        lastBuzzerToggle = currentTime;
        buzzerStep++;
        Serial.println("WARNING: Beep off");
      }
    } else {
      buzzerMode = IDLE;
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("WARNING sequence complete");
    }
  } else if (buzzerMode == SUCCESS) {
    if (buzzerStep == 0) {
      if (currentTime - lastBuzzerToggle >= 100) { // 100ms beep
        buzzerState = false;
        digitalWrite(BUZZER_PIN, LOW);
        lastBuzzerToggle = currentTime;
        buzzerStep++;
        Serial.println("SUCCESS: Beep off");
      }
    } else {
      buzzerMode = IDLE;
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("SUCCESS sequence complete");
    }
  } else if (buzzerMode == STARTUP) {
    if (buzzerStep < 4) { // 4 steps: 2 beeps (on-off-on-off)
      if (buzzerStep % 2 == 0) { // Beep on
        if (currentTime - lastBuzzerToggle >= 100) { // 100ms beep
          buzzerState = false;
          digitalWrite(BUZZER_PIN, LOW);
          lastBuzzerToggle = currentTime;
          buzzerStep++;
          Serial.println("STARTUP: Beep off, step " + String(buzzerStep));
        }
      } else { // Pause
        if (currentTime - lastBuzzerToggle >= 100) { // 100ms pause
          buzzerState = true;
          digitalWrite(BUZZER_PIN, HIGH);
          lastBuzzerToggle = currentTime;
          buzzerStep++;
          Serial.println("STARTUP: Beep on, step " + String(buzzerStep));
        }
      }
    } else {
      buzzerMode = IDLE;
      buzzerState = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("STARTUP sequence complete");
    }
  }
}