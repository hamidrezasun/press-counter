// pc_con.h - minimal console that does not use the Arduino UART driver.
//
// The Arduino core's Serial (HardwareSerial/UART driver) crashes on some ESP32 modules with this
// toolchain, which is why the firmware can be built without it (PC_NO_CONSOLE).  This console gives
// the same result anyway: output goes through the ROM's ets_printf and input is read straight from
// the UART0 RX FIFO, which the ROM already configured for 115200 8N1 to print its boot banner.
//
//   -DPC_ROM_CONSOLE   log lines and the command shell over UART0 (115200), no UART driver
#ifndef PC_CON_H
#define PC_CON_H

#include <Arduino.h>

namespace con {
void begin();          // nothing to set up: the ROM already configured UART0 at 115200
Print& out();          // Print that writes through ets_printf (ROM)
int read();            // next received byte, or -1 when the FIFO is empty
}  // namespace con

#endif
