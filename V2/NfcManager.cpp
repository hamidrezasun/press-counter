#include "NfcManager.h"

NfcManager::NfcManager() : mfrc522(CS_PIN, RST_PIN), nfc(&mfrc522), pendingTagWrite(false) {
  lastTagData[0] = '\0';
  pendingTagData[0] = '\0';
}

void NfcManager::begin() {
  SPI.begin();
  mfrc522.PCD_Init();
  nfc.begin();
}

const char* NfcManager::readTag() {
  if (nfc.tagPresent()) {
    NfcTag tag = nfc.read();
    String uid = tag.getUidString();
    strlcpy(lastTagData, uid.c_str(), MAX_TAG_DATA_LENGTH);
    
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
              char text[64] = {0};
              for (int j = 1 + languageCodeLength; j < payloadLength; j++) {
                text[j - 1 - languageCodeLength] = (char)payload[j];
              }
              strlcat(lastTagData, text, MAX_TAG_DATA_LENGTH);
            }
          }
        }
      }
    }
  }
  return lastTagData;
}

bool NfcManager::writeTag(const char* text) {
  if (nfc.tagPresent()) {
    NdefMessage message = NdefMessage();
    message.addTextRecord(text);
    bool success = nfc.write(message);
    if (success) {
      strlcpy(lastTagData, text, MAX_TAG_DATA_LENGTH);
    }
    return success;
  }
  return false;
}

const char* NfcManager::getLastTagData() const {
  return lastTagData;
}

void NfcManager::setPendingWrite(const char* text) {
  strlcpy(pendingTagData, text, MAX_TAG_DATA_LENGTH);
  pendingTagWrite = true;
}

bool NfcManager::handleNfcLoop() {
  if (pendingTagWrite && nfc.tagPresent()) {
    if (writeTag(pendingTagData)) {
      pendingTagWrite = false;
      pendingTagData[0] = '\0';
      return true;
    } else {
      pendingTagWrite = false;
      pendingTagData[0] = '\0';
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