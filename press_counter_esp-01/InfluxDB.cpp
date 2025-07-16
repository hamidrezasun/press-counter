#include "InfluxDB.h"
#include "EEPROM.h"

// Sends accumulated pulse data to InfluxDB
bool sendTo() {
  // Check if WiFi is connected and no request is in progress
  if (wifiMulti.run() != WL_CONNECTED || state.requestInProgress) {
    return false;
  }

  // Calculate total pulses to send
  uint32_t totalToSend = state.pulseCount + state.failedPulseCount;
  if (totalToSend == 0) {
    state.shouldSend = false;
    return true;
  }

  // Build InfluxDB line protocol for production data
  String lineProtocol;
  lineProtocol.reserve(128);
  lineProtocol = "production,device=" + String(DEVICE) + " count=" + String(totalToSend) + "i";
  lineProtocol += ",pulse_duration_ms=" + String(state.lastPulseDuration) + "i";
  if (state.lastSendDuration_ms > 0) {
    lineProtocol += ",send_duration_ms=" + String(state.lastSendDuration_ms) + "i";
  }
  if (state.currentDieID.length() > 0) {
    lineProtocol += ",die_id=\"" + state.currentDieID + "\"";
  }

  // Construct InfluxDB write URL
  String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + 
               "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=ms";

  // Open and send HTTP POST request
  if (request.open("POST", url.c_str())) {
    request.setReqHeader("Authorization", ("Token " + String(INFLUXDB_TOKEN)).c_str());
    request.setReqHeader("Content-Type", "text/plain; charset=utf-8");
    state.requestStartTime = millis();
    state.isDeviceInfoRequest = false;
    state.isRFIDRequest = false;
    state.requestInProgress = true;
    state.lastSendAttempt = millis();
    return request.send(lineProtocol.c_str());
  }
  
  // Handle failure to open request
  state.lastSendAttempt = millis();
  state.shouldSend = true;
  return false;
}

// Sends device information (IP and RSSI) to InfluxDB
void sendDeviceInfo() {
  // Skip if device info already sent, WiFi disconnected, or request in progress
  if (state.deviceInfoSent || wifiMulti.run() != WL_CONNECTED || state.requestInProgress) return;

  // Build InfluxDB line protocol for device info
  String lineProtocol;
  lineProtocol.reserve(128);
  lineProtocol = "device_info,device=" + String(DEVICE) + 
                 " ip=\"" + WiFi.localIP().toString() + "\"" +
                 ",rssi=" + String(WiFi.RSSI()) + "i";

  // Construct InfluxDB write URL
  String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + 
               "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=ms";

  // Open and send HTTP POST request
  if (request.open("POST", url.c_str())) {
    request.setReqHeader("Authorization", ("Token " + String(INFLUXDB_TOKEN)).c_str());
    request.setReqHeader("Content-Type", "text/plain; charset=utf-8");
    state.requestStartTime = millis();
    state.isDeviceInfoRequest = true;
    state.isRFIDRequest = false;
    state.requestInProgress = true;
    request.send(lineProtocol.c_str());
  }
}

// Sends RFID personnel data to InfluxDB
bool sendRFIDData() {
  // Check if WiFi is connected and no request is in progress
  if (wifiMulti.run() != WL_CONNECTED || state.requestInProgress) {
    return false;
  }

  // Check if there's personnel data to send
  if (!state.lastTagData.startsWith("personnel:")) {
    state.shouldSend = false;
    return true;
  }

  // Extract personnel ID
  String personnelID = state.lastTagData.substring(state.lastTagData.indexOf(':') + 1);

  // Build InfluxDB line protocol for personnel data
  String lineProtocol;
  lineProtocol.reserve(128);
  lineProtocol = "personnel_activity,device=" + String(DEVICE) + ",personnel_id=" + personnelID;
  lineProtocol += " relay_status=" + String(state.relayOn ? 1 : 0) + "i";
  if (state.currentDieID.length() > 0) {
    lineProtocol += ",die_id=\"" + state.currentDieID + "\"";
  }
  lineProtocol += ",pulse_count=" + String(state.pulseCount) + "i";

  // Construct InfluxDB write URL
  String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + 
               "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=ms";

  // Open and send HTTP POST request
  if (request.open("POST", url.c_str())) {
    request.setReqHeader("Authorization", ("Token " + String(INFLUXDB_TOKEN)).c_str());
    request.setReqHeader("Content-Type", "text/plain; charset=utf-8");
    state.requestStartTime = millis();
    state.isDeviceInfoRequest = false;
    state.isRFIDRequest = true;
    state.requestInProgress = true;
    state.lastSendAttempt = millis();
    return request.send(lineProtocol.c_str());
  }

  // Handle failure to open request
  state.lastSendAttempt = millis();
  state.shouldSend = true;
  return false;
}

// Handles HTTP request responses from InfluxDB
void onRequestResponse(void* opt, asyncHTTPrequest* req, int readyState) {
  if (readyState != 4) return; // Wait for request completion

  unsigned long sendDuration = millis() - state.requestStartTime; // Calculate send duration

  if (req->responseHTTPcode() == 204) { // Success
    if (state.isDeviceInfoRequest) {
      state.deviceInfoSent = true; // Mark device info as sent
    } else if (!state.isRFIDRequest) { // Only reset for pulse data
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
    if (!state.isDeviceInfoRequest && !state.isRFIDRequest) { // Only adjust for pulse data
      state.failedPulseCount += state.pulseCount; // Move pulses to failed count
      state.pulseCount = 0;
      saveCountsToEEPROM(); // Save updated counts
    }
    state.shouldSend = true; // Retry send
  }

  state.requestInProgress = false; // Clear request flag
  state.isDeviceInfoRequest = false; // Clear device info flag
  state.isRFIDRequest = false; // Clear RFID request flag
}