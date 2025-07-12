#include "RFID.h"

// RC522 instance with defined SS and RST pins
MFRC522 rfid(RC522_SS_PIN, RC522_RST_PIN);

// Initializes the RC522 RFID module and checks connection
void setupRFID() {
  SPI.begin();          // Initialize SPI bus
  rfid.PCD_Init();      // Initialize RC522
  state.rfidConnected = false; // Assume not connected initially

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

// Converts a hexadecimal string to a byte array
bool hexStringToByteArray(String hex, byte* buffer, size_t bufferSize) {
  if (hex.length() != bufferSize * 2) return false;
  for (size_t i = 0; i < bufferSize; i++) {
    String byteString = hex.substring(i * 2, i * 2 + 2);
    char* endPtr;
    buffer[i] = (byte)strtoul(byteString.c_str(), &endPtr, 16);
    if (endPtr == byteString.c_str()) return false; // Invalid hex
  }
  return true;
}

// Writes a new UID to the detected RFID tag
bool writeUIDToTag(String newUID) {
  if (!state.rfidConnected || !rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return false;
  }

  // Convert hex string to byte array
  byte newUidBytes[4];
  if (!hexStringToByteArray(newUID, newUidBytes, 4)) {
    return false;
  }

  // Authenticate with default key (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF)
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  byte sector = 0;
  byte blockAddr = 0; // UID is stored in block 0 of sector 0
  byte trailerBlock = 3; // Trailer block for sector 0

  // Authenticate for writing
  MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
    return false;
  }

  // Prepare data to write (UID + BCC)
  byte dataBlock[16] = {0};
  for (byte i = 0; i < 4; i++) {
    dataBlock[i] = newUidBytes[i];
  }
  dataBlock[4] = dataBlock[0] ^ dataBlock[1] ^ dataBlock[2] ^ dataBlock[3]; // Calculate BCC

  // Write the new UID to block 0
  status = rfid.MIFARE_Write(blockAddr, dataBlock, 16);
  if (status != MFRC522::STATUS_OK) {
    return false;
  }

  return true;
}

// Checks for new RFID cards and sends UID to InfluxDB
void handleRFID() {
  // Skip if RC522 is not connected
  if (!state.rfidConnected) {
    return;
  }

  // Check if a new card is present and readable
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Try to write new UID if one is set
  bool writeSuccess = false;
  if (state.newUID.length() == 8) {
    writeSuccess = writeUIDToTag(state.newUID);
    if (writeSuccess) {
      state.lastUID = state.newUID; // Update lastUID to the new value
      state.newUID = ""; // Clear the new UID after writing
    }
  }

  // Read the card's UID as a hexadecimal string
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  state.lastUID = uid; // Store UID in state

  // Toggle the relay (turn on if off, turn off if on)
  toggleRelay(!state.relayOn);

  // Send UID to InfluxDB
  if (wifiMulti.run() == WL_CONNECTED && !state.requestInProgress) {
    String measurement;
    if (uid.startsWith(String(RFID_UID_START_DIE))) {
      measurement = "rfid_tag_die";
    } else if (uid.startsWith(String(RFID_UID_START_PEOPLE))) {
      measurement = "rfid_tag_people";
    } else {
      measurement = "rfid_tag"; // Default measurement for other UIDs
    }
    String lineProtocol = measurement + ",device=" + String(DEVICE) + " uid=\"" + uid + "\"";
    if (writeSuccess) {
      lineProtocol += ",write_status=\"success\"";
    } else if (state.newUID.length() == 8) {
      lineProtocol += ",write_status=\"failed\"";
    }
    String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + 
                 "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=ms";

    if (request.open("POST", url.c_str())) {
      request.setReqHeader("Authorization", ("Token " + String(INFLUXDB_TOKEN)).c_str());
      request.setReqHeader("Content-Type", "text/plain; charset=utf-8");
      state.requestStartTime = millis();
      state.isDeviceInfoRequest = false;
      state.requestInProgress = true;
      state.lastSendAttempt = millis();
      request.send(lineProtocol.c_str());
    }
  }

  // Halt card and stop encryption
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}