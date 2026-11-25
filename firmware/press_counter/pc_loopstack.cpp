// pc_loopstack.cpp - size of the Arduino control-loop task stack (core 1).
//
// The firmware download and the flash write run inside loop(): that task owns the watchdog and it is
// the only task that touches flash.  HTTPClient + Update need more than the core's default 8192 bytes
// - the board died intermittently inside the firmware GET with that stack (see pc_trace.cpp, "stopped
// at step 3") - so the weak getArduinoLoopTaskStackSize() from the core is overridden here.
//
// Keep this definition in a .cpp file: arduino-cli's prototype generator rewrites the .ino and mangles
// a function like this one, which then fails with "expected constructor, destructor, or type
// conversion before ';' token".
#include <Arduino.h>

size_t getArduinoLoopTaskStackSize(void) { return 20480; }
