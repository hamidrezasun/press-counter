#ifndef NETWORK_H
#define NETWORK_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "define.h"
#include "filesystem.h"

ESP8266WebServer server(80);
bool apMode = false;
bool firmwareChecked = false; // Track if firmware check has been performed
unsigned long lastWiFiAttempt = 0; // Track last WiFi connection attempt

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Config</title></head><body>";
  html += "<h1>Device Configuration</h1>";
  html += "<form action='/update' method='POST'>";
  html += "WiFi SSID: <input type='text' name='wifi_ssid' value='" + String(WIFI_SSID) + "'><br>";
  html += "WiFi Password: <input type='text' name='wifi_password' value='" + String(WIFI_PASSWORD) + "'><br>";
  html += "MQTT Server: <input type='text' name='mqtt_server' value='" + String(MQTT_SERVER) + "'><br>";
  html += "MQTT Port: <input type='number' name='mqtt_port' value='" + String(MQTT_PORT) + "'><br>";
  html += "MQTT Client ID: <input type='text' name='mqtt_client_id' value='" + String(MQTT_CLIENT_ID) + "'><br>";
  html += "MQTT Button Topic: <input type='text' name='mqtt_button_topic' value='" + String(MQTT_BUTTON_TOPIC) + "'><br>";
  html += "MQTT RFID Topic: <input type='text' name='mqtt_rfid_topic' value='" + String(MQTT_RFID_TOPIC) + "'><br>";
  html += "MQTT RFID Write Topic: <input type='text' name='mqtt_rfid_write_topic' value='" + String(MQTT_RFID_WRITE_TOPIC) + "'><br>";
  html += "MQTT Device Info Topic: <input type='text' name='mqtt_device_info_topic' value='" + String(MQTT_DEVICE_INFO_TOPIC) + "'><br>";
  html += "MQTT Reconnect Interval: <input type='number' name='mqtt_reconnect_interval' value='" + String(MQTT_RECONNECT_INTERVAL) + "'><br>";
  html += "WiFi Reconnect Interval: <input type='number' name='wifi_reconnect_interval' value='" + String(WIFI_RECONNECT_INTERVAL) + "'><br>";
  html += "RFID Check Interval: <input type='number' name='rfid_check_interval' value='" + String(RFID_CHECK_INTERVAL) + "'><br>";
  html += "<input type='submit' value='Save Configuration'>";
  html += "</form>";
  html += "<h2>Firmware Update</h2>";
  html += "<form action='/firmware_update' method='POST'>";
  html += "Firmware URL: <input type='text' name='firmware_url' placeholder='http://example.com/firmware.bin'><br>";
  html += "<input type='submit' value='Update Firmware'>";
  html += "</form>";
  html += "<p>Current Firmware Version: " + String(FIRMWARE_VERSION) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleUpdate() {
  strlcpy(WIFI_SSID, server.arg("wifi_ssid").c_str(), sizeof(WIFI_SSID));
  strlcpy(WIFI_PASSWORD, server.arg("wifi_password").c_str(), sizeof(WIFI_PASSWORD));
  strlcpy(MQTT_SERVER, server.arg("mqtt_server").c_str(), sizeof(MQTT_SERVER));
  MQTT_PORT = server.arg("mqtt_port").toInt();
  strlcpy(MQTT_CLIENT_ID, server.arg("mqtt_client_id").c_str(), sizeof(MQTT_CLIENT_ID));
  strlcpy(MQTT_BUTTON_TOPIC, server.arg("mqtt_button_topic").c_str(), sizeof(MQTT_BUTTON_TOPIC));
  strlcpy(MQTT_RFID_TOPIC, server.arg("mqtt_rfid_topic").c_str(), sizeof(MQTT_RFID_TOPIC));
  strlcpy(MQTT_RFID_WRITE_TOPIC, server.arg("mqtt_rfid_write_topic").c_str(), sizeof(MQTT_RFID_WRITE_TOPIC));
  strlcpy(MQTT_DEVICE_INFO_TOPIC, server.arg("mqtt_device_info_topic").c_str(), sizeof(MQTT_DEVICE_INFO_TOPIC));
  MQTT_RECONNECT_INTERVAL = server.arg("mqtt_reconnect_interval").toInt();
  WIFI_RECONNECT_INTERVAL = server.arg("wifi_reconnect_interval").toInt();
  RFID_CHECK_INTERVAL = server.arg("rfid_check_interval").toInt();

  saveConfig();
  server.send(200, "text/html", "<h1>Configuration Saved</h1><p>Device will restart.</p>");
  delay(1000);
  ESP.restart();
}

void handleFirmwareUpdate() {
  String firmwareUrl = server.arg("firmware_url");
  if (firmwareUrl == "") {
    server.send(400, "text/html", "<h1>Error</h1><p>No firmware URL provided.</p>");
    return;
  }

  WiFiClient client;
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, firmwareUrl);
  String response;
  switch (ret) {
    case HTTP_UPDATE_OK:
      response = "<h1>Firmware Updated</h1><p>Device will restart.</p>";
      break;
    case HTTP_UPDATE_FAILED:
      response = "<h1>Firmware Update Failed</h1><p>Error: " + String(ESPhttpUpdate.getLastErrorString()) + "</p>";
      break;
    case HTTP_UPDATE_NO_UPDATES:
      response = "<h1>No Update Performed</h1><p>No new firmware available or error occurred.</p>";
      break;
  }
  server.send(200, "text/html", response);
  if (ret == HTTP_UPDATE_OK) {
    delay(1000);
    ESP.restart();
  }
}

void startWebServer(bool relayState) {
  if (!relayState || apMode) {
    server.on("/", handleRoot);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/firmware_update", HTTP_POST, handleFirmwareUpdate);
    server.begin();
  } else {
    server.stop();
  }
}

void checkFirmwareUpdate() {
  if (firmwareChecked || apMode || WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  WiFiClient client;
  http.begin(client, FIRMWARE_VERSION_URL);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String serverFirmwareName = http.getString();
    serverFirmwareName.trim();
    if (serverFirmwareName != FIRMWARE_VERSION) {
      // Firmware name mismatch, download and update
      String firmwareUrl = String(FIRMWARE_SERVER) + "/update/" + serverFirmwareName;
      t_httpUpdate_return ret = ESPhttpUpdate.update(client, firmwareUrl);
      switch (ret) {
        case HTTP_UPDATE_OK:
          // Update successful, device will restart
          break;
        case HTTP_UPDATE_FAILED:
        case HTTP_UPDATE_NO_UPDATES:
          // Handle failure or no update needed
          break;
      }
    }
  }
  http.end();
  firmwareChecked = true; // Ensure check runs only once
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    if (!firmwareChecked) {
      checkFirmwareUpdate(); // Check firmware after WiFi connection
    }
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastWiFiAttempt >= WIFI_RECONNECT_INTERVAL) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_AP);
      String ssid = "counter_" + String(ESP.getChipId());
      WiFi.softAP(ssid.c_str()); // Open AP without password
      apMode = true;
      server.on("/", handleRoot);
      server.on("/update", HTTP_POST, handleUpdate);
      server.on("/firmware_update", HTTP_POST, handleFirmwareUpdate);
      server.begin();
    } else if (!firmwareChecked) {
      checkFirmwareUpdate(); // Check firmware after successful WiFi connection
    }
    lastWiFiAttempt = currentMillis;
  }
}

void networkLoop(bool relayState) {
  if (!relayState || apMode) {
    server.handleClient();
  }
}

#endif