// NfcManager - RC522 reader.  Tags carry an NDEF text record:
//    "personnel:<id>"  operator card: toggles the relay (= starts / ends a work session)
//    "die:<id>"        die (tool) card: only recorded, never switches the machine
#ifndef NFC_MANAGER_H
#define NFC_MANAGER_H

#include <SPI.h>
#include <MFRC522.h>
#include "NfcAdapter.h"
#include "define.h"
#include "ControlManager.h"
#include "InfluxDB.h"
#include "PressCounter.h"

class NfcManager {
public:
  NfcManager();
  bool begin();                                  // false = reader not found
  void handle(ControlManager& ctrl, InfluxDB& influx, PressCounter& press);   // call every loop()

  void setPendingWrite(const char* text);        // write `text` to the next tag presented (expires after 30 s)
  bool isPendingWrite() const { return _pendingWrite; }
  const char* getLastTagData() const { return _lastTag; }

private:
  MFRC522    _rc522;
  NfcAdapter _nfc;
  char _lastTag[MAX_TAG_DATA_LENGTH]    = "";
  char _pendingText[MAX_TAG_DATA_LENGTH] = "";
  bool _pendingWrite = false;
  unsigned long _pendingSince = 0, _lastPoll = 0, _cooldownUntil = 0;

  bool readText(char* out, size_t outSize);      // first NDEF text record of the tag in the field
  bool writeText(const char* text);
};

#endif
