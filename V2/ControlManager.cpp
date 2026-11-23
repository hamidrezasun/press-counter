#include "ControlManager.h"

// on/off durations (ms), first entry = ON
static const uint16_t PAT_SUCCESS[] = {80};
static const uint16_t PAT_WARNING[] = {120, 120, 120};
static const uint16_t PAT_ERROR[]   = {300, 150, 300, 150, 300};
static const uint16_t PAT_STARTUP[] = {60, 60, 60, 60, 160};

#define RELAY_LEVEL(on) ((on) ? RELAY_ON_LEVEL : !RELAY_ON_LEVEL)

ControlManager::ControlManager(ConfigManager& cfg, InfluxDB& influx) : _config(cfg), _influx(influx) {}

void ControlManager::begin() {
  // Set the output level BEFORE switching the pin to output so the relay never glitches at boot.
  digitalWrite(RELAY_PIN, RELAY_LEVEL(false));
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);              // external 10k pull-up on the PCB
  _btnStable = _btnLast = digitalRead(BUTTON_PIN);
}

void ControlManager::handle() {
  const unsigned long now = millis();

  // ── user button: debounced, active LOW, toggles the relay on press ──────────
  const bool reading = digitalRead(BUTTON_PIN);
  if (reading != _btnLast) { _btnLast = reading; _btnChanged = now; }
  if (reading != _btnStable && now - _btnChanged >= DEBOUNCE_MS) {
    _btnStable = reading;
    if (_btnStable == LOW) toggleRelay("button");
  }

  // ── buzzer pattern ──────────────────────────────────────────────────────────
  if (_pattern && now - _patternAt >= _pattern[_patternPos]) {
    _patternPos++;
    _patternAt = now;
    if (_patternPos >= _patternLen) { _pattern = nullptr; buzzer(false); }
    else buzzer(_patternPos % 2 == 0);
  }

  // ── periodic temperature report ─────────────────────────────────────────────
  if (now - _lastTempReport >= TEMP_REPORT_MS) {
    _lastTempReport = now;
    reportTemperature();
  }
}

// ── relay ─────────────────────────────────────────────────────────────────────
void ControlManager::setRelay(bool on, const char* reason) {
  if (on == _relayOn) return;
  _relayOn = on;
  if (on) _relayOnSince = millis();
  digitalWrite(RELAY_PIN, RELAY_LEVEL(on));
  Serial.printf("[Relay] %s (%s)\n", on ? "ON" : "OFF", reason);
  char fields[48];
  snprintf(fields, sizeof(fields), "relay=%d", on ? 1 : 0);
  _influx.sendData("device_status", fields);
}

void ControlManager::toggleRelay(const char* reason) { setRelay(!_relayOn, reason); }

DeviceStatus ControlManager::getStatus() const { return {_relayOn, _buzzerOn, _btnStable == LOW}; }

// ── buzzer ────────────────────────────────────────────────────────────────────
void ControlManager::buzzer(bool on) {
  _buzzerOn = on;
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
}

void ControlManager::startPattern(const uint16_t* p, uint8_t n) {
  _pattern = p; _patternLen = n; _patternPos = 0; _patternAt = millis();
  buzzer(true);
}

void ControlManager::playSuccessSound() { startPattern(PAT_SUCCESS, sizeof(PAT_SUCCESS) / sizeof(PAT_SUCCESS[0])); }
void ControlManager::playWarningSound() { startPattern(PAT_WARNING, sizeof(PAT_WARNING) / sizeof(PAT_WARNING[0])); }
void ControlManager::playErrorSound()   { startPattern(PAT_ERROR,   sizeof(PAT_ERROR)   / sizeof(PAT_ERROR[0])); }
void ControlManager::playStartupTone()  { startPattern(PAT_STARTUP, sizeof(PAT_STARTUP) / sizeof(PAT_STARTUP[0])); }

// ── LM35 ──────────────────────────────────────────────────────────────────────
// LM35 = 10 mV per degC.  The bare ESP-12 ADC reads 0..1.0 V -> 0..1023 (ADC_FULL_SCALE_V).
float ControlManager::readTemperature() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(TEMP_SENSOR_PIN);
  const float volts = (sum / 8.0f) / 1023.0f * ADC_FULL_SCALE_V;
  return volts * 100.0f;
}

void ControlManager::reportTemperature() {
  char fields[32];
  snprintf(fields, sizeof(fields), "temperature=%.2f", readTemperature());
  _influx.sendData("temperature", fields);
}
