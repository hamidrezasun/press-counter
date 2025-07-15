#ifndef RFID_H
#define RFID_H

#include <SPI.h>
#include <MFRC522.h>
#include "defines.h"
#include "state.h"
#include "InfluxDB.h"

// External declarations for InfluxDB communication
extern asyncHTTPrequest request;

// External declaration for toggleRelay function
extern void toggleRelay(bool on);

// Function declarations for RC522 RFID operations
void setupRFID();                // Initializes the RC522 RFID module
void handleRFID();               // Checks for RFID cards and sends UID to InfluxDB
void writeRfid(String data);
#endif