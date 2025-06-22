#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <pgmspace.h>
#include "defines.h"
#include "state.h"

// External declaration for the web server object
extern ESP8266WebServer server;

// HTML template for the web interface, stored in PROGMEM to save RAM
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>%DEVICE% Status</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; }
    h1 { color: #0056b3; }
    .card { background: #fff; border: 1px solid #ddd; padding: 20px; border-radius: 8px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .status { margin-bottom: 10px; font-size: 1.1em; }
    .info { color: #5a5a5a; }
    .btn { background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 1em; }
    .btn-danger { background-color: #f44336; }
    .btn:hover { opacity: 0.9; }
    .signal-excellent { color: #28a745; font-weight: bold; }
    .signal-good { color: #8fbf2f; font-weight: bold; }
    .signal-fair { color: #fd7e14; font-weight: bold; }
    .signal-weak { color: #dc3545; font-weight: bold; }
    input[type=file] { margin-bottom: 10px; }
  </style>
</head>
<body>
  <h1>%DEVICE% Status</h1>
  <div class="card">
    <h2>Device Information</h2>
    <div class="status">WiFi Signal: <span class="%SIGNAL_CLASS%">%SIGNAL%</span></div>
    <div class="status">IP Address: %IP%</div>
  </div>
  <div class="card">
    <h2>Count Statistics & Reset</h2>
    <div class="status">Current Unsent Count: %COUNT%</div>
    <div class="status">Failed to Send Count: %FAILED%</div>
    <div class="status">Last Valid Pulse Duration: %DURATION% ms</div>
    <div class="status">Last Send Time: %SEND_DURATION% ms</div>
    <div class="status">Max Send Time: %MAX_SEND_DURATION% ms</div>
    <form method='POST' action='/reset' onsubmit="return confirm('WARNING: This will clear ALL counts and reset the maximum send time from EEPROM. This action cannot be undone. Are you sure?');" style="margin-top: 15px;">
      <button class='btn btn-danger' type='submit'>Reset Device & Clear EEPROM</button>
    </form>
  </div>
  <div class="card">
    <h2>Short Pulse Information</h2>
    <div class="status info">Last Short Pulse Duration: %SHORT_PULSE_DURATION% ms</div>
    <div class="status info">Time of Last Short Pulse: %SHORT_PULSE_TIME%</div>
    <p style="font-size:0.8em; color:#888;">This shows the last detected pulse that was too short to be counted as a valid production event (duration < %MIN_PULSE_DURATION%ms).</p>
  </div>
  <div class="card">
    <h2>Firmware Update</h2>
    <form method='POST' action='/update' enctype='multipart/form-data'>
      <input type='file' name='update'>
      <button class='btn' type='submit'>Update Firmware</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

// Function declarations for web server operations
String getSignalQuality(int rssi);     // Converts RSSI to signal quality string
void handleRoot();                     // Handles root page request
void handleReset();                    // Handles reset request
void setupWebServer();                 // Initializes the web server

#endif