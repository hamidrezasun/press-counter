// shell.h - line console for debugging and commissioning on the USB CDC port (the board's micro-USB connector J3).
#ifndef SHELL_H
#define SHELL_H

#include <Arduino.h>

namespace shell {
void begin();
void poll();
}  // namespace shell

#endif
