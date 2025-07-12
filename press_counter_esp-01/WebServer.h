#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include "defines.h"
#include "state.h"

// External declaration for the web server object
extern ESP8266WebServer server;

// Function declarations for RESTful API endpoints
String getSignalQuality(int rssi);     // Converts RSSI to signal quality string
void handleStatus();                   // Returns device status and statistics
void handleWriteRFID();               // Sets new RFID UID
void handleReset();                   // Handles reset request
void handleRelay();                   // Handles relay control requests
void setupWebServer();                // Initializes the web server

#endif