#include "RFIDHandler.h"
#include <LittleFS.h>
#include <SPI.h>
#include <Ndef.h>

extern void toggleRelay();
extern bool relay_status;

String current_die = ""; // Define global current_die

RFIDHandler::RFIDHandler() : nfc(&rfid) {
  // Constructor initializes NfcAdapter with rfid
}

void RFIDHandler::setupRFID(uint8_t ssPin, uint8_t rstPin, Network& net, Config& conf) {
  network = &net;
  config = &conf;
  SPI.begin();
  rfid.PCD_Init(ssPin, rstPin);
  nfc.begin();
}

void RFIDHandler::loopRFID() {
  if (!nfc.tagPresent()) return;

  Serial.println("Reading NFC tag");
  NfcTag tag = nfc.read();
  if (tag.hasNdefMessage()) {
    NdefMessage message = tag.getNdefMessage();
    for (unsigned int i = 0; i < message.getRecordCount(); i++) {
      NdefRecord record = message.getRecord(i); 
      if (record.getTnf() == 0x01) { // Use numeric value for TNF_WELL_KNOWN
        const byte* type = record.getType();
        if (type[0] == 'T') { // Text record
          int payloadLength = record.getPayloadLength();
          const byte* payload = record.getPayload();
          // Skip status byte (1) and language code (assume 'en', 2 bytes)
          String id_str = "";
          for (int j = 3; j < payloadLength; j++) {
            id_str += (char)payload[j];
          }

          if (id_str.startsWith("die:")) {
            String die_id = id_str.substring(4);
            saveDie(die_id);
            toggleRelay();
          } else if (id_str.startsWith("personnel:")) {
            String pers_id = id_str.substring(10);

            DynamicJsonDocument doc(256);
            if (current_die == "") {
              doc["die_id"] = JsonVariant();
            } else {
              doc["die_id"] = current_die;
            }
            doc["personnel_id"] = pers_id;
            doc["relay_status"] = relay_status;
            doc["time"] = network->getTime();
            doc["device_id"] = config->device_id;

            String json;
            serializeJson(doc, json);
            network->getMqttClient().publish(config->mqtt_nfc_topic.c_str(), json.c_str());
          }
        }
      }
    }
  }
}

void RFIDHandler::saveDie(String id) {
  File f = LittleFS.open("/die.txt", "w");
  if (f) {
    f.print(id);
    f.close();
  }
  current_die = id;
}

void RFIDHandler::loadDie() {
  if (LittleFS.exists("/die.txt")) {
    File f = LittleFS.open("/die.txt", "r");
    if (f) {
      current_die = f.readString();
      f.close();
    }
  }
}

String RFIDHandler::getCurrentDie() {
  return current_die;
}