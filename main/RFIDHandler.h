#ifndef RFIDHANDLER_H
#define RFIDHANDLER_H

#include <MFRC522.h>
#include <NfcAdapter.h>
#include <ArduinoJson.h>
#include "Network.h"
#include "Config.h"

extern String current_die; // Declare global current_die

class RFIDHandler {
private:
  MFRC522 rfid;
  NfcAdapter nfc;
  Network* network;
  Config* config;

public:
  RFIDHandler(); // Constructor to initialize NfcAdapter
  void setupRFID(uint8_t ssPin, uint8_t rstPin, Network& net, Config& conf);
  void loopRFID();
  void loadDie();
  void saveDie(String id);
  String getCurrentDie();
};

#endif