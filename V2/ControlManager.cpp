#include "ControlManager.h"

ControlManager::ControlManager(ConfigManager& cfg, InfluxDB* influx)
  : configManager(cfg), influxDB(influx), relayState(false), buzzerState(false), buttonState(false),
    lastDebounceTime(0), lastButtonReading(true), buzzerMode(IDLE), buzzerStep(0), currentFrequency(1000) {}

void ControlManager::begin() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay off initially
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button with internal pull-up
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Buzzer off initially
  pinMode(TEMP_SENSOR_PIN, INPUT); // Initialize temperature sensor pin
  Serial.println("ControlManager initialized");
}

void ControlManager::handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
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

void ControlManager::setRelayState(bool state, bool toggle, InfluxDB* influxDB) {
  if (toggle) {
    relayState = !relayState;
  } else {
    relayState = state;
  }
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
  Serial.println("Relay state set to: " + String(relayState ? "On" : "Off"));
  
  // Send to InfluxDB if provided
  if (influxDB != nullptr) {
    char fields[32];
    snprintf(fields, sizeof(fields), "state=%s", relayState ? "true" : "false");
    influxDB->sendData("relay", fields);
  }
}

void ControlManager::startBuzzer(BuzzerMode mode, int frequency) {
  if (buzzerMode != IDLE) {
    Serial.println("Buzzer already active, ignoring new mode: " + String(mode));
    return; // Prevent interrupting an ongoing buzzer sequence
  }
  buzzerMode = mode;
  buzzerStep = 0;
  buzzerState = true;
  currentFrequency = frequency;
  tone(BUZZER_PIN, currentFrequency); // Start with specified frequency
  Serial.println("Starting buzzer mode: " + String(mode) + " at frequency: " + String(frequency) + " Hz");

  // Set up Ticker based on mode
  switch (mode) {
    case BLINKING:
      buzzerTicker.attach_ms(500, [this]() { buzzerCallback(); }); // 500ms blink interval
      break;
    case ERROR:
      buzzerTicker.attach_ms(200, [this]() { buzzerCallback(); }); // 200ms for error beeps
      break;
    case WARNING:
      buzzerTicker.attach_ms(500, [this]() { buzzerCallback(); }); // 500ms for warning beep
      break;
    case SUCCESS:
      buzzerTicker.attach_ms(100, [this]() { buzzerCallback(); }); // 100ms for success beep
      break;
    case STARTUP:
      buzzerTicker.attach_ms(150, [this]() { buzzerCallback(); }); // 150ms for startup beeps
      break;
    default:
      stopBuzzer();
      break;
  }
}

void ControlManager::stopBuzzer() {
  buzzerMode = IDLE;
  buzzerState = false;
  noTone(BUZZER_PIN);
  buzzerTicker.detach();
}

void ControlManager::buzzerCallback() {
  if (buzzerMode == IDLE) {
    stopBuzzer();
    return;
  }

  if (buzzerMode == BLINKING) {
    buzzerState = !buzzerState;
    if (buzzerState) {
      tone(BUZZER_PIN, currentFrequency); // Use stored frequency
    } else {
      noTone(BUZZER_PIN);
    }
    Serial.println("Blinking buzzer toggled: " + String(buzzerState ? "On" : "Off"));
  } else if (buzzerMode == ERROR) {
    if (buzzerStep < 6) { // 6 steps: 3 beeps (on-off-on-off-on-off)
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(BUZZER_PIN, currentFrequency); // Use stored frequency
      } else {
        noTone(BUZZER_PIN);
      }
      buzzerStep++;
    } else {
      stopBuzzer();
      Serial.println("ERROR sequence complete");
    }
  } else if (buzzerMode == WARNING) {
    if (buzzerStep == 0) {
      buzzerState = false;
      noTone(BUZZER_PIN);
      buzzerStep++;
    } else {
      stopBuzzer();
      Serial.println("WARNING sequence complete");
    }
  } else if (buzzerMode == SUCCESS) {
    if (buzzerStep == 0) {
      buzzerState = false;
      noTone(BUZZER_PIN);
      buzzerStep++;
    } else {
      stopBuzzer();
      Serial.println("SUCCESS sequence complete");
    }
  } else if (buzzerMode == STARTUP) {
    if (buzzerStep < 4) { // 4 steps: 2 beeps (on-off-on-off)
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(BUZZER_PIN, currentFrequency); // Use stored frequency
      } else {
        noTone(BUZZER_PIN);
      }
      buzzerStep++;
    } else {
      stopBuzzer();
      Serial.println("STARTUP sequence complete");
    }
  }
}

float ControlManager::readTemperature() {
  int raw = analogRead(TEMP_SENSOR_PIN);      // Read ADC (0-1023)
  float voltage = (raw / 1023.0) * 1.0;      // Convert to voltage (0-1 V)
  float temperatureC = voltage * 100.0;      // LM35: 10 mV/°C, max 100°C
  return temperatureC;
}

void ControlManager::sendTemperatureToInflux() {
  if (influxDB == nullptr) {
    Serial.println("InfluxDB not initialized, cannot send temperature");
    return;
  }
  float temperature = readTemperature();
  char fields[32];
  snprintf(fields, sizeof(fields), "temperature=%.2f", temperature);
  influxDB->sendData("temperature", fields);
  Serial.println("Sent temperature to InfluxDB: " + String(temperature) + " °C");
}

void ControlManager::startTemperatureReporting() {
  if (influxDB == nullptr) {
    Serial.println("Cannot start temperature reporting: InfluxDB not initialized");
    return;
  }
  temperatureTicker.attach(60, [this]() { sendTemperatureToInflux(); }); // Send every 60 seconds
  Serial.println("Started temperature reporting every 60 seconds");
}

void ControlManager::stopTemperatureReporting() {
  temperatureTicker.detach();
  Serial.println("Stopped temperature reporting");
}

void ControlManager::playErrorSound(int frequency) {
  startBuzzer(ERROR, frequency);
}

void ControlManager::playWarningSound(int frequency) {
  startBuzzer(WARNING, frequency);
}

void ControlManager::playSuccessSound(int frequency) {
  startBuzzer(SUCCESS, frequency);
}

void ControlManager::playStartupTone(int frequency) {
  startBuzzer(STARTUP, frequency);
}