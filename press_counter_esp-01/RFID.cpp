#include <SPI.h>
#include <MFRC522.h>
#include "RFID.h"
#include "NfcAdapter.h"

// RC522 instance with defined SS and RST pins
MFRC522 rfid(RC522_SS_PIN, RC522_RST_PIN);

NfcAdapter nfc = NfcAdapter(&rfid); // Fixed: Use 'rfid' instead of 'mfrc522'

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
if (nfc.tagPresent()) {
        Serial.println("Writing record to NFC tag");
        NdefMessage message = NdefMessage();
        message.addTextRecord(data.c_str());
        bool success = nfc.write(message);
        if (success) {
          state.attemptToWrite = false;
        }
    }
      
}

void handleRFID() {
  if (nfc.tagPresent()) {
    toggleRelay(!state.relayOn);
    state.lastTagData = "";  // Clear previous tag data
    NfcTag tag = nfc.read();
    state.lastUID = tag.getUidString();  // Set UID as a string
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
            }
          }
        }
      }
    }
  }
}