// pc_nfc.h - RC522 RFID reader: reads / writes one NDEF text record per card (MIFARE Classic, NTAG, Ultralight).
//   personnel:<id>  -> toggles the machine (session start / stop)
//   die:<id>        -> records the die (tool) only
#ifndef PC_NFC_H
#define PC_NFC_H

#include "pc_config.h"

namespace nfc {
typedef void (*TagCallback)(char kind, const char* text);        // kind: pc::NFC_PERSONNEL / NFC_DIE / NFC_UNKNOWN / NFC_WRITTEN

bool begin();                                  // true if the reader answers
void poll();                                   // call every loop
void onTag(TagCallback cb);
bool present();                                // reader answered on its last health check
void queueWrite(const char* text);             // the next card presented is written (30 s window)
void cancelWrite();
bool writePending();
uint32_t writeRemainingMs();
const char* lastTag();                         // text of the last card read ("" = none)
char kindOf(const char* text);                 // classifies "personnel:..." / "die:..." / other
}  // namespace nfc

#endif
