#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#include <Arduino.h>
#include <Ticker.h>
#include "ConfigManager.h"
#include "InfluxDB.h"
#include "define.h"

struct DeviceStatus {
  bool relayOn;
  bool buzzerOn;
  bool buttonPressed;
};

class ControlManager {
public:
  ControlManager(ConfigManager& cfg, InfluxDB* influx = nullptr);
  void begin();
  void handleButton();
  DeviceStatus getStatus();
  void setRelayState(bool state, bool toggle = false, InfluxDB* influxDB = nullptr);
  void playErrorSound(int frequency = 1000);
  void playWarningSound(int frequency = 1000);
  void playSuccessSound(int frequency = 1000);
  void playStartupTone(int frequency = 1000);
  void buzzerLoop(); // Non-blocking buzzer handling
  float readTemperature();
  void startTemperatureReporting();
  void stopTemperatureReporting();
  void shift74HC595(uint8_t binaryData); // New method to send raw binary to 74HC595

private:
  ConfigManager& configManager;
  InfluxDB* influxDB; // Store InfluxDB instance
  bool relayState;
  bool buzzerState;
  bool buttonState;
  unsigned long lastDebounceTime;
  bool lastButtonReading;
  enum BuzzerMode { IDLE, BLINKING, ERROR, WARNING, SUCCESS, STARTUP };
  BuzzerMode buzzerMode;
  int buzzerStep;
  int currentFrequency; // Store current frequency
  Ticker buzzerTicker;
  Ticker temperatureTicker; // Ticker for temperature reporting
  void startBuzzer(BuzzerMode mode, int frequency = 1000);
  void buzzerCallback(); // Callback for Ticker
  void stopBuzzer();
  void sendTemperatureToInflux(); // Method for sending temperature
};

#endif