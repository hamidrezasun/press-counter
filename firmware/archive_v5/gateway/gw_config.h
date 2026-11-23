// gw_config.h - constants of the ESP8266 gateway firmware.
#ifndef GW_CONFIG_H
#define GW_CONFIG_H

#include <Arduino.h>
#include "pc_proto.h"

// ESP-12F pins on the v4 MAIN board (see hardware/tools/design.py)
#define PIN_MCU_NRST   4        // GPIO4 -> 470R -> STM32 NRST (open-drain style: INPUT = released, OUTPUT LOW = reset)
#define PIN_MCU_BOOT0  5        // GPIO5 -> 1k -> STM32 BOOT0 (10k pull-down on the board): HIGH = boot the ROM bootloader
#define PIN_WIFI_LED   2        // blue LED to 3V3: LOW = lit (GPIO2 must be HIGH at boot)
// UART0 is moved to GPIO13 (RX) / GPIO15 (TX) with Serial.swap(): that is the link to the STM32 (header H1 keeps GPIO1 / GPIO3
// for flashing this ESP).

#define LINK_BAUD          38400
#define LINK_ALIVE_MS      6000UL
#define NET_STATUS_MS      2000UL
#define TIME_SYNC_MS       600000UL
#define MQTT_RETRY_MS      10000UL
#define LOG_LINES          24
#define LOG_LINE_LEN       96

#define GW_VERSION         PC_FW_VERSION_STR
#define STM32_FLASH_BASE   0x08000000UL
#define STM32_IMAGE_PATH   "/stm32.bin"
#define STM32_MAX_IMAGE    (62UL * 1024UL)

#endif
