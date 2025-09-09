#ifndef NFC_MANAGER_H
#define NFC_MANAGER_H

#include <SPI.h>
#include <MFRC522.h>
#include "NfcAdapter.h"
#include "define.h"

#define MAX_TAG_DATA_LENGTH 128

// Forward declarations
class WebServerManager;
class ControlManager;
class InfluxDB; // Added forward declaration

class NfcManager {
public:
  NfcManager();
  bool begin();
  const char* readTag();
  bool writeTag(const char* text);
  const char* getLastTagData() const;
  void setPendingWrite(const char* text);
  bool handleNfcLoop(WebServerManager& webManager, ControlManager& controlManager, InfluxDB& influxDB); // Modified
  bool isPendingWrite() const;

private:
 MFRC522 mfrc522;
    NfcAdapter nfc;
    char lastTagData[MAX_TAG_DATA_LENGTH];
    char pendingTagData[MAX_TAG_DATA_LENGTH];
    bool pendingTagWrite;
};

#endif