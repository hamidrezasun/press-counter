#ifndef RFID_H
#define RFID_H

#include <SPI.h>
#include <MFRC522.h>
#include "NfcAdapter.h"
#include "define.h"
#include "mqtt.h"

MFRC522 mfrc522(RC522_SS_PIN, RC522_RST_PIN); // Create MFRC522 instance
NfcAdapter nfc = NfcAdapter(&mfrc522);         // Create NfcAdapter instance

void setupRFID() {
  SPI.begin();           // Initialize SPI bus
  mfrc522.PCD_Init();    // Initialize MFRC522
  nfc.begin();           // Initialize NfcAdapter
}

String readRFID() {
  if (!nfc.tagPresent()) {
    return "";
  }

  NfcTag tag = nfc.read();
  String tagData = tag.getTagType() + " UID:" + tag.getUidString();
  return tagData;
}

bool writeRFID(const String& data) {
  if (!nfc.tagPresent()) {
    return false;
  }

  NdefMessage message = NdefMessage();
  message.addUriRecord(data.c_str());
  return nfc.write(message);
}

#endif