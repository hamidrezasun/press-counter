// bootloader.h - reset-cause capture and the software route into the STM32 ROM UART bootloader (AN2606, USART1 PA9/PA10).
// The normal route used by the ESP gateway is hardware: BOOT0 high (ESP GPIO5) + NRST pulse (ESP GPIO4).  The software route
// below lets a node reboot into the ROM bootloader on request when those wires are not fitted.
#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "config.h"

namespace bootloader {
void earlyInit();                  // call first thing after reset (initVariant): captures the reset cause, honours a pending ROM-boot request
void enterRom();                   // reboot into the ROM bootloader (does not return)
bool wasWatchdogReset();
}  // namespace bootloader

#endif
