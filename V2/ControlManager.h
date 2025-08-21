#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "define.h"

struct DeviceStatus {
  bool relayOn;
  bool buzzerOn;
  bool buttonPressed;
};

class ControlManager {
public:
  ControlManager(ConfigManager& cfg);
  void begin();
  void handleButton();
  DeviceStatus getStatus();
  void setRelayState(bool state, bool toggle = false);
  void playErrorSound();
  void playWarningSound();
  void playSuccessSound();
  void playStartupTone();
  void buzzerLoop(); // Non-blocking buzzer handling

private:
  ConfigManager& configManager;
  bool relayState;
  bool buzzerState;
  bool buttonState;
  unsigned long lastDebounceTime;
  bool lastButtonReading;
  enum BuzzerMode { IDLE, BLINKING, ERROR, WARNING, SUCCESS, STARTUP };
  BuzzerMode buzzerMode;
  unsigned long buzzerStartTime;
  unsigned long lastBuzzerToggle;
  int buzzerStep;
  void startBuzzer(BuzzerMode mode);
};

#endif