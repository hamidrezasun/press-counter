#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <asyncHTTPrequest.h>
#include <EEPROM.h>
#include "WebServer.h"
#include "defines.h"
#include "state.h"
#include "InfluxDB.h"
#include "RFID.h"

// Global objects for WiFi, web server, and HTTP requests
ESP8266WiFiMulti wifiMulti;          // Manages WiFi connections
ESP8266WebServer server(80);         // Web server on port 80
ESP8266HTTPUpdateServer httpUpdater; // HTTP server for firmware updates
asyncHTTPrequest request;            // Asynchronous HTTP request handler

// Global state variable to track device status and pulse counts
State state = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false, 0, false, false, "", "", false, ""};

// Toggles the relay on or off and updates state
void toggleRelay(bool on) {
  state.relayOn = on; // Update relay status in state
  digitalWrite(RELAY_PIN, on ? HIGH : LOW); // Set relay pin (HIGH = on, LOW = off)
}

// Saves pulse counts and max send duration to EEPROM if changed
void saveCountsToEEPROM() {
  static uint32_t lastSavedPulseCount = 0;    // Last saved pulse count
  static uint32_t lastSavedFailedCount = 0;   // Last saved failed count
  static uint32_t lastSavedMaxDuration = 0;   // Last saved max send duration
  
  // Check if any values have changed
  if (state.pulseCount != lastSavedPulseCount || 
      state.failedPulseCount != lastSavedFailedCount ||
      state.maxSendDuration_ms != lastSavedMaxDuration) {
    
    EEPROM.begin(EEPROM_SIZE); // Initialize EEPROM
    EEPROM.write(0, INIT_MARKER); // Write initialization marker
    EEPROM.put(1, state.pulseCount); // Save pulse count
    EEPROM.put(5, state.failedPulseCount); // Save failed count
    EEPROM.put(9, state.maxSendDuration_ms); // Save max send duration
    EEPROM.commit(); // Commit changes
    EEPROM.end(); // Release EEPROM
    
    // Update last saved values
    lastSavedPulseCount = state.pulseCount;
    lastSavedFailedCount = state.failedPulseCount;
    lastSavedMaxDuration = state.maxSendDuration_ms;
  }
}

// Loads pulse counts and max send duration from EEPROM
void loadCountsFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE); // Initialize EEPROM
  if (EEPROM.read(0) != INIT_MARKER) { // Check for initialization marker
    EEPROM.write(0, INIT_MARKER); // Write marker if uninitialized
    EEPROM.put(1, 0); // Initialize pulse count
    EEPROM.put(5, 0); // Initialize failed count
    EEPROM.put(9, 0); // Initialize max send duration
    EEPROM.commit(); // Commit changes
    state.pulseCount = 0;
    state.failedPulseCount = 0;
    state.maxSendDuration_ms = 0;
  } else {
    // Load values from EEPROM
    EEPROM.get(1, state.pulseCount);
    EEPROM.get(5, state.failedPulseCount);
    EEPROM.get(9, state.maxSendDuration_ms);
  }
  EEPROM.end(); // Release EEPROM
}

// Resets pulse counts and max send duration, saving to EEPROM
void resetEEPROM() {
  state.pulseCount = 0;
  state.failedPulseCount = 0;
  state.maxSendDuration_ms = 0;
  saveCountsToEEPROM(); // Save reset values
}

// Detects and processes pulse trains from the input pin
void handlePulseTrain() {
  static int lastState = LOW; // Last state of the input pin
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
      if (state.pulseCount % REPORT_INTERVAL == 0) {
        state.shouldSend = true; // Trigger send to InfluxDB
      }
    } else {
      // Record short pulse
      state.lastShortPulseDuration = duration;
      state.lastShortPulseTime = millis();
    }
  }
}

// Handles HTTP request responses from InfluxDB
void onRequestResponse(void* opt, asyncHTTPrequest* req, int readyState) {
  if (readyState != 4) return; // Wait for request completion

  unsigned long sendDuration = millis() - state.requestStartTime; // Calculate send duration

  if (req->responseHTTPcode() == 204) { // Success
    if (state.isDeviceInfoRequest) {
      state.deviceInfoSent = true; // Mark device info as sent
    } else {
      state.pulseCount = 0; // Clear pulse counts
      state.failedPulseCount = 0;
      saveCountsToEEPROM(); // Save cleared counts
    }
    state.lastSendDuration_ms = sendDuration; // Record send duration
    if (sendDuration > state.maxSendDuration_ms) {
      state.maxSendDuration_ms = sendDuration; // Update max duration
      saveCountsToEEPROM(); // Save new max duration
    }
    state.shouldSend = false; // Clear send flag
  } else { // Failure
    state.lastSendAttempt = millis(); // Record attempt time
    if (!state.isDeviceInfoRequest) {
      state.failedPulseCount += state.pulseCount; // Move pulses to failed count
      state.pulseCount = 0;
      saveCountsToEEPROM(); // Save updated counts
    }
    state.shouldSend = true; // Retry send
  }

  state.requestInProgress = false; // Clear request flag
  state.isDeviceInfoRequest = false; // Clear device info flag
}

// Initializes the device and sets up peripherals
void setup() {
  ESP.wdtEnable(8000); // Enable watchdog timer with 8-second timeout
  pinMode(INPUT_PIN, INPUT); // Configure pulse input pin
  pinMode(RELAY_PIN, OUTPUT); // Configure relay pin as output
  toggleRelay(false); // Initialize relay to off
 
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
  handlePulseTrain(); // Process pulse input
  if (!state.attemptToWrite){
    handleRFID(); // Process RFID card reads
  }else{
    writeRfid(state.newTagData);
  }
  // Check if data should be sent to InfluxDB
  if (state.shouldSend && !state.inPulseTrain && !state.requestInProgress) {
    if (millis() - state.lastSendAttempt >= SEND_RETRY_INTERVAL) {
      sendTo(); // Attempt to send pulse data
    }
  }
}