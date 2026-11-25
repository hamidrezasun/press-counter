// pc_update.h - firmware updates from the user's own server (docs/FIRMWARE.md), besides the USB cable and ArduinoOTA.
//
//   <updateUrl>/manifest.json   {"esp32": {"version":"6.1","url":"press_counter-6.1.bin","md5":"<hex>","size":123456}}
//
//   The image is streamed into the OTA slot (Update class), MD5 checked, then the board restarts.  An update is installed in
//   every situation: if the machine relay is on it is switched off first (safe state), an active stroke signal is tolerated.
//   Not cryptographically signed: use a server you trust on a network you trust.
#ifndef PC_UPDATE_H
#define PC_UPDATE_H

#include <Arduino.h>

namespace fwupd {
enum Phase : uint8_t { P_IDLE = 0, P_CHECK = 1, P_DOWNLOAD = 2, P_FLASH = 3, P_FAILED = 4, P_DONE = 5 };

struct Info {
  uint8_t phase;
  uint8_t pct;
  char msg[60];
  char latest[12];                      // version found in the manifest ("" = unknown)
  bool isNew;
};

void begin();
void loop();
void request(bool apply);               // queue a manifest check; apply = install a newer version
const Info& info();
bool busy();
}  // namespace fwupd

#endif
