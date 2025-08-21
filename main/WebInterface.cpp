#include "WebInterface.h"

WebInterface::WebInterface() : server(80) {
  // Initialize server in constructor
}

void WebInterface::setupWeb(Config& conf) {
  config = &conf;

  server.on("/", [this]() {
    String html = "<html><body><form action='/save' method='post'>";
    html += "NTP Server: <input name='ntp_server' value='" + config->ntp_server + "'><br>";
    html += "Debounce Delay: <input name='debounce_delay' value='" + String(config->debounce_delay) + "'><br>";
    html += "Device ID: <input name='device_id' value='" + config->device_id + "'><br>";
    html += "WiFi SSID: <input name='wifi_ssid' value='" + config->wifi_ssid + "'><br>";
    html += "WiFi Password: <input name='wifi_password' value='" + config->wifi_password + "'><br>";
    html += "MQTT Server: <input name='mqtt_server' value='" + config->mqtt_server + "'><br>";
    html += "MQTT Port: <input name='mqtt_port' value='" + String(config->mqtt_port) + "'><br>";
    html += "MQTT Counting Topic: <input name='mqtt_counting_topic' value='" + config->mqtt_counting_topic + "'><br>";
    html += "MQTT NFC Topic: <input name='mqtt_nfc_topic' value='" + config->mqtt_nfc_topic + "'><br>";
    html += "MQTT Device Info Topic: <input name='mqtt_device_info_topic' value='" + config->mqtt_device_info_topic + "'><br>";
    html += "MQTT Turnon Topic: <input name='mqtt_turnon_topic' value='" + config->mqtt_turnon_topic + "'><br>";
    html += "MQTT Username: <input name='mqtt_username' value='" + config->mqtt_username + "'><br>";
    html += "MQTT Password: <input name='mqtt_password' value='" + config->mqtt_password + "'><br>";
    html += "<input type='submit' value='Save'></form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/save", [this]() {
    config->ntp_server = server.arg("ntp_server");
    config->debounce_delay = server.arg("debounce_delay").toInt();
    config->device_id = server.arg("device_id");
    config->wifi_ssid = server.arg("wifi_ssid");
    config->wifi_password = server.arg("wifi_password");
    config->mqtt_server = server.arg("mqtt_server");
    config->mqtt_port = server.arg("mqtt_port").toInt();
    config->mqtt_counting_topic = server.arg("mqtt_counting_topic");
    config->mqtt_nfc_topic = server.arg("mqtt_nfc_topic");
    config->mqtt_device_info_topic = server.arg("mqtt_device_info_topic");
    config->mqtt_turnon_topic = server.arg("mqtt_turnon_topic");
    config->mqtt_username = server.arg("mqtt_username");
    config->mqtt_password = server.arg("mqtt_password");
    config->save();
    server.send(200, "text/html", "Saved. Restarting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

void WebInterface::loop() {
  server.handleClient();
}