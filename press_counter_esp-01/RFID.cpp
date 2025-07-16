#include <SPI.h>
#include <MFRC522.h>
#include "RFID.h"
#include "NfcAdapter.h"
#include "EEPROM.h"

// RC522 instance with defined SS and RST pins
MFRC522 rfid(RC522_SS_PIN, RC522_RST_PIN);

NfcAdapter nfc = NfcAdapter(&rfid);

// Initializes the RC522 RFID module and checks connection
void setupRFID() {
  SPI.begin();          // Initialize SPI bus
  rfid.PCD_Init();      // Initialize RC522
  state.rfidConnected = false; // Assume not connected initially
  nfc.begin();

  // Perform self-test to verify RC522 connection
  for (int i = 0; i < 3; i++) { // Retry up to 3 times
    if (rfid.PCD_PerformSelfTest()) {
      state.rfidConnected = true; // Mark as connected if self-test passes
      break;
    }
    delay(100); // Wait before retry
  }

  // Maximize antenna gain for better range
  if (state.rfidConnected) {
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  }
}

void writeRfid(String data) {
  // Validate tagtype:id format
  int colonIndex = data.indexOf(':');
  if (colonIndex == -1) {
    Serial.println("Invalid RFID data format: Missing colon");
    return;
  }
  String tagType = data.substring(0, colonIndex);
  String id = data.substring(colonIndex + 1);
  if (tagType != "die" && tagType != "personnel") {
    Serial.println("Invalid RFID tag type: Must be 'die' or 'personnel'");
    return;
  }
  if (id.length() == 0) {
    Serial.println("Invalid RFID ID: ID cannot be empty");
    return;
  }
  for (int i = 0; i < id.length(); i++) {
    char c = id.charAt(i);
    if (!isAlphaNumeric(c)) {
      Serial.println("Invalid RFID ID: Must contain only alphanumeric characters");
      return;
    }
  }

  if (nfc.tagPresent()) {
    Serial.println("Writing record to NFC tag: " + data);
    NdefMessage message = NdefMessage();
    message.addTextRecord(data.c_str());
    bool success = nfc.write(message);
    if (success) {
      Serial.println("Successfully wrote to NFC tag");
      state.attemptToWrite = false;
    } else {
      Serial.println("Failed to write to NFC tag");
    }
  } else {
    Serial.println("No NFC tag present for writing");
  }
}

void handleRFID() {
  if (nfc.tagPresent()) {
    unsigned long currentTime = millis();
    // Debounce: Only process tag if at least 1000ms have passed since last read
    if (currentTime - state.lastTagReadTime < 1000) {
      return; // Ignore tag if read too recently
    }
    state.lastTagReadTime = currentTime; // Update last read time
    toggleRelay(!state.relayOn); // Toggle relay once
    state.lastTagData = "";     // Clear previous tag data
    state.lastUID = "";         // Clear previous UID
    NfcTag tag = nfc.read();
    state.lastUID = tag.getUidString(); // Set UID as a string
    if (tag.hasNdefMessage()) {
      NdefMessage message = tag.getNdefMessage();
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++) {
        NdefRecord record = message.getRecord(i);
        // Check if the record is a text record (TNF = 0x01 and type = "T")
        if (record.getTnf() == 0x01 && record.getTypeLength() == 1 && record.getType()[0] == 'T') {
          const byte* payload = record.getPayload();
          int payloadLength = record.getPayloadLength();
          if (payloadLength > 0) {
            byte status = payload[0];
            int languageCodeLength = status & 0x3F;  // Lower 6 bits of status byte
            if (payloadLength > 1 + languageCodeLength) {
              String text = "";
              for (int j = 1 + languageCodeLength; j < payloadLength; j++) {
                text += (char)payload[j];
              }
              if (state.lastTagData.length() > 0) {
                state.lastTagData += " ";  // Separate multiple records with a space
              }
              state.lastTagData += text;
              
              // Parse tagtype:id format
              int colonIndex = text.indexOf(':');
              if (colonIndex != -1) {
                String tagType = text.substring(0, colonIndex);
                String id = text.substring(colonIndex + 1);
                if (tagType == "die") {
                  // Store die ID in EEPROM (max 15 chars)
                  state.currentDieID = id.substring(0, min((int)id.length(), 15));
                  saveCountsToEEPROM();
                } else if (tagType == "personnel") {
                  // Trigger InfluxDB send for personnel data
                  state.shouldSend = true;
                  // Store personnel ID in lastTagData
                  state.lastTagData = text;
                }
              }
            }
          }
        }
      }
    }
  }
}