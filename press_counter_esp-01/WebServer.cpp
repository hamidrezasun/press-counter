#include "WebServer.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "state.h"

// External declarations for dependencies
extern ESP8266HTTPUpdateServer httpUpdater; // HTTP update server for firmware updates
extern void resetEEPROM();                  // Function to reset EEPROM data

// Converts WiFi RSSI to a descriptive signal quality string
String getSignalQuality(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  return "Weak";
}

// Handles requests to the root URL, serving the status page
void handleRoot() {
  String page = FPSTR(htmlPage); // Load HTML template from PROGMEM
  int rssi = WiFi.RSSI();        // Get current WiFi signal strength
  
  // Determine signal quality class for CSS styling
  String signalClass = "signal-weak";
  if (rssi >= -50) signalClass = "signal-excellent";
  else if (rssi >= -60) signalClass = "signal-good";
  else if (rssi >= -70) signalClass = "signal-fair";

  // Format time since last short pulse
  String shortPulseTime = "Never";
  if (state.lastShortPulseTime > 0) {
    unsigned long seconds = (millis() - state.lastShortPulseTime) / 1000;
    shortPulseTime = String(seconds) + " seconds ago";
  }

  // Replace placeholders in HTML template with actual values
  page.replace("%DEVICE%", DEVICE);
  page.replace("%SIGNAL%", getSignalQuality(rssi) + " (" + String(rssi) + " dBm)");
  page.replace("%SIGNAL_CLASS%", signalClass);
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%COUNT%", String(state.pulseCount));
  page.replace("%FAILED%", String(state.failedPulseCount));
  page.replace("%DURATION%", String(state.lastPulseDuration));
  page.replace("%SEND_DURATION%", String(state.lastSendDuration_ms));
  page.replace("%MAX_SEND_DURATION%", String(state.maxSendDuration_ms));
  page.replace("%SHORT_PULSE_DURATION%", String(state.lastShortPulseDuration));
  page.replace("%SHORT_PULSE_TIME%", shortPulseTime);
  page.replace("%MIN_PULSE_DURATION%", String(MIN_PULSE_TRAIN_DURATION));

  // Send the rendered page to the client
  server.send(200, "text/html", page);
}

// Handles POST requests to /reset, clearing EEPROM and redirecting to root
void handleReset() {
  resetEEPROM(); // Clear counts and max send duration in EEPROM
  server.sendHeader("Location", "/"); // Redirect to root page
  server.send(303); // HTTP 303 See Other
}

// Initializes the web server and sets up routes
void setupWebServer() {
  server.on("/", handleRoot); // Route for root page
  server.on("/reset", HTTP_POST, handleReset); // Route for reset action
  httpUpdater.setup(&server, "/update", "admin", "password"); // Setup firmware update route
  server.begin(); // Start the web server
}