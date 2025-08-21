#include "filesystem.h"
#include "network.h"
#include "mqtt.h"
#include "rfid.h"

int buttonCount = 0;          // Counter for button presses (sum since relay HIGH)
int buttonState = HIGH;       // Current state of the button
int lastButtonState = HIGH;   // Previous state of the button
unsigned long lastDebounceTime = 0;  // Last time the button was toggled
unsigned long lastRFIDCheck = 0;     // Last time RFID was checked
unsigned long buttonPressStart = 0;  // Start time of button press
unsigned long minButtonDuration = ULONG_MAX; // Minimum button press duration
unsigned long maxButtonDuration = 0; // Maximum button press duration
bool relayState = HIGH;       // Relay state (HIGH by default)

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Set button pin as input with pull-up
  pinMode(RELAY_PIN, OUTPUT);        // Set relay pin as output
  digitalWrite(RELAY_PIN, relayState); // Initialize relay to HIGH
  initFilesystem();                  // Initialize SPIFFS
  loadConfig();                      // Load configuration
  setupRFID();                       // Initialize RFID
  connectToWiFi();
  connectToMQTT();
  startWebServer(relayState);        // Start web server based on relay state
}

void loop() {
  if (relayState) {
    // Handle button press detection
    int reading = digitalRead(BUTTON_PIN); // Read the state of the button

    if (reading != lastButtonState) {
      lastDebounceTime = millis(); // Record the time of the change
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (reading != buttonState) {
        buttonState = reading;
        if (buttonState == LOW) { // Button pressed and relay is HIGH
          buttonCount++;
          buttonPressStart = millis();
        } else if (buttonState == HIGH && buttonPressStart != 0) { // Button released
          unsigned long duration = millis() - buttonPressStart;
          minButtonDuration = min(minButtonDuration, duration);
          maxButtonDuration = max(maxButtonDuration, duration);
          sendButtonToMQTT(buttonCount, duration, minButtonDuration, maxButtonDuration);
          buttonPressStart = 0;
        }
      }
    }
    lastButtonState = reading;
  }

  // Handle RFID reading and writing (non-blocking)
  if (millis() - lastRFIDCheck >= RFID_CHECK_INTERVAL) {
    String writeData = getRFIDWriteData();
    if (writeData != "") {
      // Write mode: write received MQTT data to tag
      writeRFID(writeData);
    } else {
      // Read mode: read tag, toggle relay, and send to MQTT
      String rfidData = readRFID();
      if (rfidData != "") {
        relayState = !relayState; // Toggle relay state
        digitalWrite(RELAY_PIN, relayState);
        if (!relayState) { // Relay is LOW, reset button count
          buttonCount = 0;
          minButtonDuration = ULONG_MAX;
          maxButtonDuration = 0;
        }
        sendRFIDToMQTT(rfidData, relayState);
        startWebServer(relayState); // Update web server state
      }
    }
    lastRFIDCheck = millis();
  }

  // Maintain MQTT and network connections non-blocking
  mqttLoop();
  networkLoop(relayState);
}