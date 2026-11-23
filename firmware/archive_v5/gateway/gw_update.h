// gw_update.h - firmware updates fetched from the user's own server (docs/FIRMWARE.md).
//
//   <updateUrl>/manifest.json   {"esp":   {"version":"3.1","url":"gateway-3.1.bin","md5":"<hex>","size":123456},
//                                "stm32": {"version":"3.1","url":"counter-3.1.bin","crc32":"<hex>","size":45678}}
//
//   * ESP8266 image: streamed straight into the OTA slot (Update class), MD5 checked, then reboot.
//   * STM32 image: stored in LittleFS (/stm32.bin), CRC32 checked, flashed through the ROM bootloader (UART, AN3155) and read back.
//     Only done while the machine is idle (relay off, no stroke) - or when the STM32 does not answer at all (recovery).
//     A marker file makes an interrupted flash (power loss) restart by itself after the next boot.
//   * Not cryptographically signed: use a server you trust on a network you trust (Bearer token + MD5 / CRC32 only protect
//     against transmission errors and casual access).
#ifndef GW_UPDATE_H
#define GW_UPDATE_H

#include <Arduino.h>

namespace fwupd {

// numbers are the "fwstate" of the N frame to the STM32 (display shows them)
enum Phase : uint8_t { P_IDLE = 0, P_CHECK = 1, P_DOWNLOAD = 2, P_FLASH = 3, P_FAILED = 4, P_DONE = 5 };

struct Info {
  uint8_t phase;
  uint8_t pct;
  char msg[60];
  char espLatest[12], stmLatest[12];    // versions found in the manifest ("" = unknown)
  bool espNew, stmNew;
  bool manifestOk;
  uint32_t lastCheckS;                  // uptime seconds of the last check, 0 = never
};

typedef void (*TickFn)();
void begin(TickFn tick);                // tick() is called about once a second while a long job runs (sends the N frame)
void loop();
void request(bool apply);               // queue a manifest check; apply = install what is newer
void requestStmRetry();                 // flash the image stored in /stm32.bin again
const Info& info();
bool busy();
}  // namespace fwupd

#endif
