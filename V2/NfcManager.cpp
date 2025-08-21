#include "NfcManager.h"

NfcManager::NfcManager() : mfrc522(CS_PIN, RST_PIN), nfc(&mfrc522), lastTagData(""), pendingTagData(""), pendingTagWrite(false) {}

void NfcManager::begin() {
  SPI.begin();
  mfrc522.PCD_Init();
  nfc.begin();
}

String NfcManager::readTag() {
  if (nfc.tagPresent()) {
    NfcTag tag = nfc.read();
    lastTagData = tag.getUidString();
    
    Serial.println("NFC Tag Read:");
    Serial.print("UID: ");
    Serial.println(lastTagData);
    Serial.print("Tag Type: ");
    Serial.println(tag.getTagType());
    if (tag.hasNdefMessage()) {
      NdefMessage message = tag.getNdefMessage();
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++) {
        NdefRecord record = message.getRecord(i);
        if (record.getTnf() == 0x01 && record.getTypeLength() == 1 && record.getType()[0] == 'T') {
          const byte* payload = record.getPayload();
          int payloadLength = record.getPayloadLength();
          if (payloadLength > 0) {
            byte status = payload[0];
            int languageCodeLength = status & 0x3F;
            if (payloadLength > 1 + languageCodeLength) {
              String text = "";
              for (int j = 1 + languageCodeLength; j < payloadLength; j++) {
                text += (char)payload[j];
              }
              lastTagData += text;
            }
          }
        }
      }
    }
  }
  return lastTagData;
}

bool NfcManager::writeTag(const String& text) {
  if (nfc.tagPresent()) {
    NdefMessage message = NdefMessage();
    message.addTextRecord(text.c_str());
    bool success = nfc.write(message);
    if (success) {
      lastTagData = text;
    }
    return success;
  }
  return false;
}

String NfcManager::getLastTagData() const {
  return lastTagData;
}

void NfcManager::setPendingWrite(const String& text) {
  pendingTagData = text;
  pendingTagWrite = true;
}

bool NfcManager::handleNfcLoop() {
  if (pendingTagWrite && nfc.tagPresent()) {
    if (writeTag(pendingTagData)) {
      pendingTagWrite = false;
      pendingTagData = "";
      return true;
    } else {
      pendingTagWrite = false;
      pendingTagData = "";
      return false;
    }
  } else {
    readTag();
    return false;
  }
}

bool NfcManager::isPendingWrite() const {
  return pendingTagWrite;
}