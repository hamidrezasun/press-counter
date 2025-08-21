#include "WebServer.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "state.h"
#include "InfluxDB.h"

// External declarations for dependencies
extern ESP8266HTTPUpdateServer httpUpdater; // HTTP update server for firmware updates
extern void resetEEPROM();                  // Function to reset EEPROM data
extern void toggleRelay(bool on);           // Function to toggle relay state
extern void saveCountsToEEPROM();          // Function to save counts and die ID to EEPROM
extern bool sendRFIDData();                // Function to send RFID data to InfluxDB

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

// Validates die ID or personnel ID (alphanumeric, non-empty)
bool isValidID(String id) {
  if (id.length() == 0) return false;
  for (int i = 0; i < id.length(); i++) {
    char c = id.charAt(i);
    if (!isAlphaNumeric(c)) {
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
  String rfidStatus = state.rfidConnected ? "Connected" : "Not Connected";
  String attemptToWrite = state.attemptToWrite ? "trying" : "done";
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
  json += "\"min_pulse_duration\":" + String(MIN_PULSE_TRAIN_DURATION) + ",";
  json += "\"last_edge_time\":" + String(state.lastEdgeTime) + ",";
  json += "\"pulse_train_start\":" + String(state.pulseTrainStart) + ",";
  json += "\"last_send_attempt\":" + String(state.lastSendAttempt) + ",";
  json += "\"in_pulse_train\":" + String(state.inPulseTrain ? "true" : "false") + ",";
  json += "\"should_send\":" + String(state.shouldSend ? "true" : "false") + ",";
  json += "\"device_info_sent\":" + String(state.deviceInfoSent ? "true" : "false") + ",";
  json += "\"request_in_progress\":" + String(state.requestInProgress ? "true" : "false") + ",";
  json += "\"is_device_info_request\":" + String(state.isDeviceInfoRequest ? "true" : "false") + ",";
  json += "\"request_start_time\":" + String(state.requestStartTime) + ",";
  json += "\"is_rfid_request\":" + String(state.isRFIDRequest ? "true" : "false") + ",";
  json += "\"rfid_status\":\"" + rfidStatus + "\",";
  json += "\"last_uid\":\"" + (state.lastUID.length() > 0 ? state.lastUID : "None") + "\",";
  json += "\"last_tag_data\":\"" + (state.lastTagData.length() > 0 ? state.lastTagData : "None") + "\",";
  json += "\"new_tag_data\":\"" + (state.newTagData.length() > 0 ? state.newTagData : "None") + "\",";
  json += "\"current_die_id\":\"" + (state.currentDieID.length() > 0 ? state.currentDieID : "None") + "\",";
  json += "\"attempt_write_tag\":\"" + attemptToWrite + "\",";
  json += "\"opto_pin_status\":\"" + String(digitalRead(INPUT_PIN)) + "\",";
  json += "\"last_tag_read_time\":" + String(state.lastTagReadTime);
  json += "}";
  server.send(200, "application/json", json);
}

// Handles POST /write_rfid, storing the new UID
void handleWriteRFID() {
  if (server.hasArg("newTagData")) {
    state.attemptToWrite = true;
    String newTagData = server.arg("newTagData");
    newTagData.trim();
    state.newTagData = newTagData; // Store valid UID
    server.send(200, "application/json", "{\"status\":\"success\",\"new_Tag_Data\":\"" + newTagData + "\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No newTagData provided\"}");
  }
}

// Handles POST /set_die_id, setting the die ID
void handleSetDieID() {
  if (server.hasArg("dieID")) {
    String dieID = server.arg("dieID");
    dieID.trim();
    if (isValidID(dieID) && dieID.length() <= 15) {
      state.currentDieID = dieID.substring(0, min((int)dieID.length(), 15)); // Limit to 15 chars
      saveCountsToEEPROM(); // Save new die ID to EEPROM
      server.send(200, "application/json", "{\"status\":\"success\",\"die_id\":\"" + state.currentDieID + "\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid dieID: Must be alphanumeric, 1-15 characters\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"No dieID provided\"}");
  }
}

// Handles POST /set_personnel_id, setting the personnel ID
void handleSetPersonnelID() {
  if (server.hasArg("personnelID")) {
    String personnelID = server.arg("personnelID");
    personnelID.trim();
    if (isValidID(personnelID)) {
      state.lastTagData = "personnel:" + personnelID; // Store in lastTagData with personnel prefix
      server.send(200, "application/json", "{\"status\":\"success\",\"personnel_id\":\"" + personnelID + "\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid personnelID: Must be alphanumeric and non-empty\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"No personnelID provided\"}");
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
      if (state.lastTagData.startsWith("personnel:")) {
        sendRFIDData(); // Send RFID data if personnel ID is set
      }
      server.send(200, "application/json", "{\"status\":\"Relay turned on\"}");
    } else if (action == "off") {
      toggleRelay(false);
      if (state.lastTagData.startsWith("personnel:")) {
        sendRFIDData(); // Send RFID data if personnel ID is set
      }
      server.send(200, "application/json", "{\"status\":\"Relay turned off\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Invalid action: must be 'on' or 'off'\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"No action provided\"}");
  }
}

// Serves the Vue web interface at root
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// Initializes the web server and sets up routes
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot); // Serve web interface
  server.on("/status", HTTP_GET, handleStatus); // Device status and statistics
  server.on("/write_rfid", HTTP_POST, handleWriteRFID); // Set new RFID UID
  server.on("/set_die_id", HTTP_POST, handleSetDieID); // Set new die ID
  server.on("/set_personnel_id", HTTP_POST, handleSetPersonnelID); // Set new personnel ID
  server.on("/reset", HTTP_POST, handleReset);  // Reset EEPROM
  server.on("/relay", HTTP_POST, handleRelay);  // Relay control
  httpUpdater.setup(&server, "/update", "admin", "password"); // Firmware update
  server.begin();
}