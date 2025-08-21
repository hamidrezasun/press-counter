#ifndef NFC_MANAGER_H
#define NFC_MANAGER_H

#include <SPI.h>
#include <MFRC522.h>
#include "NfcAdapter.h"
#include "define.h"

class NfcManager {
public:
  NfcManager();
  void begin();
  String readTag();
  bool writeTag(const String& text);
  String getLastTagData() const;
  void setPendingWrite(const String& text);
  bool handleNfcLoop();
  bool isPendingWrite() const;

private:
  MFRC522 mfrc522;
  NfcAdapter nfc;
  String lastTagData;
  String pendingTagData;
  bool pendingTagWrite;
};

#endif