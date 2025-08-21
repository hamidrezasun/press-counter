#include "InfluxDB.h"
#include "EEPROM.h"

// Sends accumulated pulse data to InfluxDB
bool sendTo() {
  // Check if WiFi is connected and no request is in progress
  if (wifiMulti.run() != WL_CONNECTED || state.requestInProgress) {
    return false;
  }

  // Calculate total pulses to send (current + failed)
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
  lineProtocol += " relay_status=" + String(!state.relayOn ? 1 : 0) + "i";
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
  if (readyState != 4) return;

  unsigned long sendDuration = millis() - state.requestStartTime;

  if (req->responseHTTPcode() == 204) { // Success
    if (state.isDeviceInfoRequest) {
      state.deviceInfoSent = true;
    } else if (!state.isRFIDRequest) {
      state.pulseCount = 0;
      state.failedPulseCount = 0;
      saveCountsToEEPROM();
    }
    state.lastSendDuration_ms = sendDuration;
    if (sendDuration > state.maxSendDuration_ms) {
      state.maxSendDuration_ms = sendDuration;
      saveCountsToEEPROM();
    }
    state.shouldSend = false;
  } else { // Failure
    if (!state.isDeviceInfoRequest && !state.isRFIDRequest) {
      state.failedPulseCount += state.pulseCount;
      state.pulseCount = 0;
      saveCountsToEEPROM();
    }
    state.shouldSend = true; // Retry on next loop
  }

  state.requestInProgress = false;
  state.isDeviceInfoRequest = false;
  state.isRFIDRequest = false;
}
