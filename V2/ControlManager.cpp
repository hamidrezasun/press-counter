#include "ControlManager.h"

ControlManager::ControlManager(ConfigManager& cfg, InfluxDB* influx)
  : configManager(cfg), influxDB(influx), relayState(false), buzzerState(false),
    buttonState(false), lastDebounceTime(0), lastButtonReading(false),
    buzzerMode(IDLE), buzzerStep(0), currentFrequency(1000),
    temperatureTicker() {
}

void ControlManager::begin() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(DIN_MAX, OUTPUT); // 74HC595 DS (data)
  pinMode(CLK_MAX, OUTPUT); // 74HC595 SHCP (shift clock)
  pinMode(CS_MAX, OUTPUT);  // 74HC595 STCP (latch)
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(CS_MAX, HIGH); // Latch high by default
  Serial.println("ControlManager initialized with 74HC595");
}

void ControlManager::handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        setRelayState(!relayState, true, influxDB);
      }
    }
  }
  lastButtonReading = reading;
}

DeviceStatus ControlManager::getStatus() {
  return {relayState, buzzerState, buttonState};
}

void ControlManager::setRelayState(bool state, bool toggle, InfluxDB* influxDB) {
  relayState = toggle ? !relayState : state;
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
  if (influxDB) {
    char fields[32];
    snprintf(fields, sizeof(fields), "relay=%d", relayState ? 1 : 0);
    influxDB->sendData("device_status", fields);
  }
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

void ControlManager::startBuzzer(BuzzerMode mode, int frequency) {
  buzzerMode = mode;
  currentFrequency = frequency;
  buzzerStep = 0;
  buzzerState = false;
  buzzerTicker.detach();
  buzzerTicker.attach_ms(100, [this]() { buzzerCallback(); });
}

void ControlManager::buzzerCallback() {
  switch (buzzerMode) {
    case ERROR:
      if (buzzerStep < 6) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
        buzzerStep++;
      } else {
        stopBuzzer();
      }
      break;
    case WARNING:
      if (buzzerStep < 4) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
        buzzerStep++;
      } else {
        stopBuzzer();
      }
      break;
    case SUCCESS:
      if (buzzerStep < 2) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
        buzzerStep++;
      } else {
        stopBuzzer();
      }
      break;
    case STARTUP:
      if (buzzerStep < 8) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
        buzzerStep++;
      } else {
        stopBuzzer();
      }
      break;
    default:
      stopBuzzer();
      break;
  }
}

void ControlManager::stopBuzzer() {
  buzzerMode = IDLE;
  buzzerState = false;
  digitalWrite(BUZZER_PIN, LOW);
  buzzerTicker.detach();
}

void ControlManager::buzzerLoop() {
  // Handled by Ticker
}

float ControlManager::readTemperature() {
  int raw = analogRead(TEMP_SENSOR_PIN);
  float voltage = raw * (3.3 / 1023.0);
  float temperature = (voltage - 0.5) * 100; // Example conversion for TMP36
  return temperature;
}

void ControlManager::startTemperatureReporting() {
  temperatureTicker.attach(60, [this]() { sendTemperatureToInflux(); });
}

void ControlManager::stopTemperatureReporting() {
  temperatureTicker.detach();
}

void ControlManager::sendTemperatureToInflux() {
  if (influxDB) {
    float temperature = readTemperature();
    char fields[32];
    snprintf(fields, sizeof(fields), "temperature=%.2f", temperature);
    influxDB->sendData("temperature", fields);
  }
}

void ControlManager::shift74HC595(uint8_t binaryData) {
  digitalWrite(CS_MAX, LOW); // Latch low to prepare for shifting
  shiftOut(DIN_MAX, CLK_MAX, MSBFIRST, binaryData); // Shift out 8-bit binary data
  digitalWrite(CS_MAX, HIGH); // Latch high to output data
}