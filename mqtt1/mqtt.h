#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "define.h"
#include "filesystem.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMQTTAttempt = 0;
String rfidWriteData = ""; // Store latest RFID data received from MQTT for writing
bool mqttConnected = false; // Track MQTT connection state

// Function prototypes
void sendStoredCounts(const char* filename, const char* topic);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, MQTT_RFID_WRITE_TOPIC) == 0) {
    rfidWriteData = "";
    for (unsigned int i = 0; i < length; i++) {
      rfidWriteData += (char)payload[i];
    }
    rfidWriteData.trim();
  }
}

void sendDeviceInfo(unsigned long mqttLatency) {
  String info = "{";
  info += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  info += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  info += "\"mqtt_latency\":" + String(mqttLatency) + ",";
  info += "\"chip_id\":" + String(ESP.getChipId()) + ",";
  info += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
  info += "\"free_heap\":" + String(ESP.getFreeHeap());
  info += "}";
  mqttClient.publish(MQTT_DEVICE_INFO_TOPIC, info.c_str());
}

void connectToMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastMQTTAttempt >= MQTT_RECONNECT_INTERVAL) {
    unsigned long startTime = millis();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      mqttClient.subscribe(MQTT_RFID_WRITE_TOPIC);
      if (!mqttConnected) {
        sendDeviceInfo(millis() - startTime); // Send device info on first connect
        mqttConnected = true;
      }
      sendStoredCounts(BUTTON_COUNT_FILE, MQTT_BUTTON_TOPIC);
      sendStoredCounts(RFID_COUNT_FILE, MQTT_RFID_TOPIC);
    }
    lastMQTTAttempt = currentMillis;
  }
}

void saveCountToFile(const String& data, const char* filename) {
  File file = SPIFFS.open(filename, "a");
  if (file) {
    file.println(data);
    file.close();
  }
}

void sendStoredCounts(const char* filename, const char* topic) {
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      mqttClient.publish(topic, line.c_str());
    }
  }
  file.close();
  SPIFFS.remove(filename); // Clear the file after sending
}

void sendButtonToMQTT(int sum, unsigned long duration, unsigned long minDuration, unsigned long maxDuration) {
  String data = "{\"count\":1,\"sum\":" + String(sum) + ",\"duration\":" + String(duration) +
                ",\"minDuration\":" + String(minDuration) + ",\"maxDuration\":" + String(maxDuration) + "}";
  if (!mqttClient.connected()) {
    saveCountToFile(data, BUTTON_COUNT_FILE); // Save only if MQTT is unavailable
    connectToMQTT();
    return;
  }
  if (!mqttClient.publish(MQTT_BUTTON_TOPIC, data.c_str())) {
    saveCountToFile(data, BUTTON_COUNT_FILE); // Save only if publish fails
  }
}

void sendRFIDToMQTT(const String& rfidData, bool relayState) {
  if (rfidData == "") {
    return;
  }
  String data = "{\"tag\":\"" + rfidData + "\",\"relay\":" + (relayState ? "1" : "0") + "}";
  if (!mqttClient.connected()) {
    saveCountToFile(data, RFID_COUNT_FILE); // Save only if MQTT is unavailable
    connectToMQTT();
    return;
  }
  if (!mqttClient.publish(MQTT_RFID_TOPIC, data.c_str())) {
    saveCountToFile(data, RFID_COUNT_FILE); // Save only if publish fails
  }
}

void mqttLoop() {
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
}

String getRFIDWriteData() {
  return rfidWriteData;
}

#endif