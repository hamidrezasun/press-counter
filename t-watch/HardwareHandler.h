#ifndef HARDWAREHANDLER_H
#define HARDWAREHANDLER_H

#include "config.h"

class HardwareHandler {
public:
  void init();
  void toggleVibrate();
  void vibrate(uint32_t duration = 500);
  void playBeep(uint32_t frequency = 1000, uint32_t duration = 200);

private:
  bool vibrateState = false;
};

#endif