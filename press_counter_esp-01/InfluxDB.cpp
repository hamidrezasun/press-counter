#include "InfluxDB.h"

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

  // Construct InfluxDB write URL
  String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + 
               "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=ms";

  // Open and send HTTP POST request
  if (request.open("POST", url.c_str())) {
    request.setReqHeader("Authorization", ("Token " + String(INFLUXDB_TOKEN)).c_str());
    request.setReqHeader("Content-Type", "text/plain; charset=utf-8");
    state.requestStartTime = millis();
    state.isDeviceInfoRequest = false;
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
    state.requestInProgress = true;
    request.send(lineProtocol.c_str());
  }
}