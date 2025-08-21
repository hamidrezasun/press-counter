#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <asyncHTTPrequest.h>
#include <EEPROM.h>
#include "WebServer.h"
#include "defines.h"
#include "state.h"
#include "InfluxDB.h"
#include "RFID.h"
#include "EEPROM.h"

// Global objects for WiFi, web server, and HTTP requests
ESP8266WiFiMulti wifiMulti;          // Manages WiFi connections
ESP8266WebServer server(80);         // Web server on port 80
ESP8266HTTPUpdateServer httpUpdater; // HTTP server for firmware updates
asyncHTTPrequest request;            // Asynchronous HTTP request handler

// Global state variable to track device status and pulse counts
State state = {0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false, 0, false, false, "", "", false, "", "", false, 0};

// Toggles the relay on or off and updates state
void toggleRelay(bool on) {
  state.relayOn = on; // Update relay status in state
  digitalWrite(RELAY_PIN, on ? HIGH : LOW); // Set relay pin (HIGH = on, LOW = off)
}

// Detects and processes pulse trains from the input pin
void handlePulseTrain() {
  static int lastState = HIGH; // Last state of the input pin
  int currentState = digitalRead(INPUT_PIN); // Current state of the input pin

  // Detect edge change
  if (currentState != lastState) {
    state.lastEdgeTime = millis(); // Record edge time
    if (!state.inPulseTrain) { // Start new pulse train
      state.inPulseTrain = true;
      state.pulseTrainStart = state.lastEdgeTime;
    }
    lastState = currentState; // Update last state
  }

  // Check if pulse train has ended
  if (state.inPulseTrain && (millis() - state.lastEdgeTime > EDGE_TIMEOUT)) {
    unsigned long duration = state.lastEdgeTime - state.pulseTrainStart;
    state.inPulseTrain = false; // End pulse train

    // Process valid pulse train
    if (duration >= MIN_PULSE_TRAIN_DURATION) {
      state.pulseCount++; // Increment pulse count
      state.lastPulseDuration = duration; // Record duration
      state.shouldSend = true; // Trigger send to InfluxDB
    } else {
      // Record short pulse
      state.lastShortPulseDuration = duration;
    }
  }
}

// Initializes the device and sets up peripherals
void setup() {
  pinMode(INPUT_PIN, INPUT); // Configure pulse input pin
  pinMode(RELAY_PIN, OUTPUT); // Configure relay pin as output
  toggleRelay(true); // Initialize relay to off
 
  loadCountsFromEEPROM(); // Load saved counts from EEPROM

  setupRFID(); // Initialize RC522 RFID module

  WiFi.mode(WIFI_STA); // Set WiFi to station mode
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD); // Add WiFi credentials
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable WiFi sleep

  // Wait for WiFi connection
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
  }

  request.setTimeout(HTTP_REQUEST_TIMEOUT); // Set HTTP request timeout
  request.onReadyStateChange(onRequestResponse); // Set request callback
  setupWebServer(); // Initialize web server
  
  sendDeviceInfo(); // Send initial device info
  if (state.pulseCount > 0 || state.failedPulseCount > 0) {
    state.shouldSend = true; // Trigger send if counts exist
  }
}

// Main loop to handle web server, pulse detection, RFID, and data sending
void loop() {
  server.handleClient(); // Process web server requests
  
  // Call handlePulseTrain if relay is off and personnel ID is set, or always for normal pulse detection
  if (!state.relayOn && state.lastTagData.startsWith("personnel:")) {
    handlePulseTrain(); // Process pulse input
  }
  
  if (!state.attemptToWrite) {
    handleRFID(); // Process RFID card reads
  } else {
    writeRfid(state.newTagData);
  }
  
  // Check if data should be sent to InfluxDB
  if (state.shouldSend && !state.inPulseTrain && !state.requestInProgress) sendTo(); 
}