#include "NfcManager.h"
#include "WebServerManager.h"
#include "ControlManager.h"
#include "InfluxDB.h"
#include "PressCounter.h"  // Add include
#include <Arduino.h>

NfcManager::NfcManager() : mfrc522(CS_PIN, -1), nfc(&mfrc522), pendingTagWrite(false) {
  lastTagData[0] = '\0';
  pendingTagData[0] = '\0';
}

bool NfcManager::begin() {
  SPI.begin();
  mfrc522.PCD_Init();
  nfc.begin();
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  return (v != 0x00 && v != 0xFF && mfrc522.PCD_PerformSelfTest());
}

const char* NfcManager::readTag() {
  NfcTag tag = nfc.read();
  String uid = tag.getUidString();
  //strlcpy(lastTagData, uid.c_str(), MAX_TAG_DATA_LENGTH);    
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
            strlcpy(lastTagData, text, MAX_TAG_DATA_LENGTH);
          }
        }
      }
    }
  }
  return lastTagData;
}
bool NfcManager::writeTag(const char* text) {
  // Reset the reader to ensure clean state
  mfrc522.PCD_Reset();
  mfrc522.PCD_Init();
  
  // Multiple attempts to detect tag
  for (int i = 0; i < 5; i++) {
    Serial.print("Attempting to write: ");
    Serial.println(text);
    
    NdefMessage message = NdefMessage();
    message.addTextRecord(text);
    bool success = nfc.write(message);
    
    if (success) {
      Serial.println("Successfully wrote data on Tag");
      strlcpy(lastTagData, text, MAX_TAG_DATA_LENGTH);
      return true;
    } else {
      Serial.println("Failed to write to tag");
      // Reset and try again
      mfrc522.PCD_Reset();
      mfrc522.PCD_Init();
      delay(100);
    }
  }
  return false;
}

const char* NfcManager::getLastTagData() const {
  return lastTagData;
}

void NfcManager::setPendingWrite(const char* text) {
  strlcpy(pendingTagData, text, MAX_TAG_DATA_LENGTH);
  pendingTagWrite = true;
  Serial.print("Attemt to write: ");
  Serial.println(text);
}

bool NfcManager::handleNfcLoop(WebServerManager& webManager, ControlManager& controlManager, 
                              InfluxDB& influxDB, PressCounter& pressCounter) {  // Add PressCounter parameter
  bool result = false;
  
  // Handle pending write first (highest priority)
  if (pendingTagWrite && nfc.tagPresent()) {
    if (writeTag(pendingTagData)) {
      pendingTagWrite = false;
      pendingTagData[0] = '\0';
      result = true;
      controlManager.setRelayState(true, true);
      controlManager.playSuccessSound();
      pressCounter.reset();  // Reset press counter on successful write
    } else {
      Serial.println("Write failed");
      pendingTagWrite = false;
      pendingTagData[0] = '\0';
      result = false;
    }
  } else if (!pendingTagWrite && nfc.tagPresent()) {
    // Read tag
    const char* currentTag = readTag();
    bool hasTagData = (strlen(currentTag) > 0);
    if (hasTagData) {
      result = true;
      Serial.print("Tag Data: ");
      Serial.println(currentTag);
      controlManager.setRelayState(true, true);
      controlManager.playSuccessSound(1500);
      
      // Reset press counter when tag is detected
      pressCounter.reset();
      
      // Send tag data and relay status to InfluxDB
      DeviceStatus status = controlManager.getStatus();
      char fields[256];
      snprintf(fields, sizeof(fields), "tag_data=\"%s\",action=\"read\",relay_state=%s", 
               currentTag, status.relayOn ? "true" : "false");
      influxDB.sendData("nfc_event", fields);
    }
  }
  // Handle web updates
  if (result || strlen(getLastTagData()) > 0) {
    webManager.updateStatus();
  }
  
  return result;
}


bool NfcManager::isPendingWrite() const {
  return pendingTagWrite;
}