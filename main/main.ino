#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include "Config.h"
#include "Network.h"
#include "RFIDHandler.h"
#include "PressCounter.h"
#include "WebInterface.h"

#define RELAY_PIN 0   // Updated pin for relay
#define BUZZER_PIN 15 // Updated pin for buzzer
#define OPTO_PIN 2    // Updated pin for optocoupler
#define RFID_SS_PIN 4 // Updated SS pin for MFRC522
#define RFID_RST_PIN 5 // Updated RST pin for MFRC522
#define BUTTON_PIN 16 // New pin for button

Config config;
Network network;
RFIDHandler rfidHandler;
PressCounter pressCounter;
WebInterface webInterface;

bool relay_status = false; // Shared relay status, false = off
bool last_button_state = HIGH; // Button is active LOW
unsigned long button_press_time = 0;
bool button_held_sent = false;
const unsigned long hold_duration = 1000; // 1 second for hold
const unsigned long debounce_delay = 50; // Debounce delay for button

void setup() {
  Serial.begin(74880);
  delay(1000); // Give hardware time to stabilize
  Serial.println("Starting setup...");
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(OPTO_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Setup button pin
  digitalWrite(RELAY_PIN, HIGH);

  Serial.println("Mounting LittleFS...");
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    failureMode();
    return; // Ensure we don't proceed if LittleFS fails
  }

  Serial.println("Loading configuration...");
  if (!config.load()) {
    Serial.println("Failed to load configuration, entering failure mode");
    failureMode();
    return;
  }

  Serial.println("Setting up network...");
  network.setup(config); // Pass config to network

  Serial.println("Connecting to network...");
  if (!network.connectAll()) {
    Serial.println("Network connection failed, entering failure mode");
    failureMode();
    return;
  }
  Serial.println("IP Address: " + WiFi.localIP().toString());
  
  Serial.println("Setting up web interface...");
  webInterface.setupWeb(config);

  Serial.println("Sending device info...");
  if (network.sendDeviceInfo()) {
    Serial.println("Device info sent successfully");
  } else {
    Serial.println("Failed to send device info, continuing...");
    failureMode();
    return;
  }

  Serial.println("Setting up RFID handler...");
  rfidHandler.setupRFID(RFID_SS_PIN, RFID_RST_PIN, network, config);
  rfidHandler.loadDie(); // Load current die if exists

  Serial.println("Setting up press counter...");
  pressCounter.setupPress(OPTO_PIN, config.debounce_delay, network, config);

  Serial.println("Setup completed. Free heap: " + String(ESP.getFreeHeap()));
}

void loop() {
  //network.loop();
  //rfidHandler.loopRFID();
  //pressCounter.loopPress();
  //if (!config.counting) {
    webInterface.loop();
  //}
  //handleButton();
}

void failureMode() {
  Serial.println("Entering failure mode...");
  digitalWrite(RELAY_PIN, HIGH); // Turn relay on

  // Start Soft AP
  WiFi.mode(WIFI_AP);
  String apSSID = "ESP_Press_Config_" + String(ESP.getChipId(), HEX);
  const char* apPassword = "config123"; // Hardcoded password for simplicity
  WiFi.softAP(apSSID.c_str(), apPassword);
  Serial.println("Soft AP started: " + apSSID + " Password: " + apPassword);
  Serial.println("IP Address: " + WiFi.softAPIP().toString());

  // Start web server for configuration
  webInterface.setupWeb(config);

  // Blink buzzer and handle web server
  while (true) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);
    webInterface.loop(); // Handle web server requests
  }
}

void toggleRelay() {
  relay_status = !relay_status;
  digitalWrite(RELAY_PIN, relay_status ? HIGH : LOW);
  Serial.println("Relay toggled to: " + String(relay_status ? "ON" : "OFF"));
}

void setRelay(bool state) {
  relay_status = state;
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  Serial.println("Relay set to: " + String(state ? "ON" : "OFF"));
}

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  static unsigned long last_debounce_time = 0;

  if (reading != last_button_state) {
    last_debounce_time = millis();
  }

  if ((millis() - last_debounce_time) > debounce_delay) {
    if (reading == LOW && last_button_state == HIGH) { // Button pressed
      button_press_time = millis();
      button_held_sent = false;
    } else if (reading == LOW && !button_held_sent && (millis() - button_press_time) >= hold_duration) {
      // Button held
      network.sendButtonSignal("hold");
      button_held_sent = true;
    } else if (reading == HIGH && last_button_state == LOW && !button_held_sent) {
      // Button released before hold duration
      network.sendButtonSignal("pressed");
    }
    last_button_state = reading;
  }
}