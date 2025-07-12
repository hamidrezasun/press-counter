#include "WebServer.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "state.h"

// External declarations for dependencies
extern ESP8266HTTPUpdateServer httpUpdater; // HTTP update server for firmware updates
extern void resetEEPROM();                  // Function to reset EEPROM data
extern void toggleRelay(bool on);           // Function to toggle relay state

// Validates if a string is an 8-character hexadecimal value
bool isValidHexUID(String uid) {
  if (uid.length() != 8) return false;
  for (int i = 0; i < 8; i++) {
    char c = uid.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

// Converts WiFi RSSI to a descriptive signal quality string
String getSignalQuality(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  return "Weak";
}

// Handles GET /status, returning device status and statistics
void handleStatus() {
  int rssi = WiFi.RSSI();
  String shortPulseTime = state.lastShortPulseTime > 0 ? String((millis() - state.lastShortPulseTime) / 1000) + " seconds ago" : "Never";
  String rfidStatus = state.rfidConnected ? "Connected" : "Not Connected";

  String json = "{";
  json += "\"device\":\"" + String(DEVICE) + "\",";
  json += "\"wifi_signal\":\"" + getSignalQuality(rssi) + " (" + String(rssi) + " dBm)\",";
  json += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"relay_status\":\"" + String(state.relayOn ? "On" : "Off") + "\",";
  json += "\"unsent_count\":" + String(state.pulseCount) + ",";
  json += "\"failed_count\":" + String(state.failedPulseCount) + ",";
  json += "\"last_pulse_duration\":" + String(state.lastPulseDuration) + ",";
  json += "\"last_send_duration\":" + String(state.lastSendDuration_ms) + ",";
  json += "\"max_send_duration\":" + String(state.maxSendDuration_ms) + ",";
  json += "\"last_short_pulse_duration\":" + String(state.lastShortPulseDuration) + ",";
  json += "\"last_short_pulse_time\":\"" + shortPulseTime + "\",";
  json += "\"min_pulse_duration\":" + String(MIN_PULSE_TRAIN_DURATION) + ",";
  json += "\"rfid_status\":\"" + rfidStatus + "\",";
  json += "\"last_uid\":\"" + (state.lastUID.length() > 0 ? state.lastUID : "None") + "\",";
  json += "\"new_uid\":\"" + (state.newUID.length() > 0 ? state.newUID : "None") + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

// Handles POST /write_rfid, storing the new UID
void handleWriteRFID() {
  if (server.hasArg("new_uid")) {
    String newUID = server.arg("new_uid");
    newUID.trim();
    newUID.toUpperCase();
    // Validate: must be 8 characters, hexadecimal
    if (isValidHexUID(newUID)) {
      state.newUID = newUID; // Store valid UID
      server.send(200, "application/json", "{\"status\":\"success\",\"new_uid\":\"" + newUID + "\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid UID: Must be 8 hexadecimal characters (e.g., 1A2B3C4D)\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"No UID provided\"}");
  }
}

// Handles POST /reset, clearing EEPROM
void handleReset() {
  resetEEPROM();
  server.send(200, "application/json", "{\"status\":\"EEPROM reset successfully\"}");
}

// Handles POST /relay, toggling the relay state
void handleRelay() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    if (action == "on") {
      toggleRelay(true);
      server.send(200, "application/json", "{\"status\":\"Relay turned on\"}");
    } else if (action == "off") {
      toggleRelay(false);
      server.send(200, "application/json", "{\"error\":\"Relay turned off\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid action: must be 'on' or 'off'\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"No action provided\"}");
  }
}

// Initializes the web server and sets up routes
void setupWebServer() {
  server.on("/status", HTTP_GET, handleStatus); // Device status and statistics
  server.on("/write_rfid", HTTP_POST, handleWriteRFID); // Set new RFID UID
  server.on("/reset", HTTP_POST, handleReset);  // Reset EEPROM
  server.on("/relay", HTTP_POST, handleRelay);  // Relay control
  httpUpdater.setup(&server, "/update", "admin", "password"); // Firmware update
  server.begin();
}