#include "Network.h"
#include <ArduinoJson.h>
#include "PressCounter.h"

extern PressCounter pressCounter;
extern Config config;
extern void setRelay(bool state);

Network* Network::instance = nullptr;

void Network::setup(Config& conf) {
  config = &conf;
  setInstance(this);
  timeClient = new NTPClient(ntpUDP, config->ntp_server.c_str(), 0, 15000);
  mqttClient = PubSubClient(wifiClient);
  mqttClient.setServer(config->mqtt_server.c_str(), config->mqtt_port);
  mqttClient.setCallback(callback);
  mqttClient.setKeepAlive(120); // Increased to 120s to avoid timeouts
  mqttClient.setSocketTimeout(120); // Timeout for MQTT operations
  Serial.println("Network setup completed. Free heap: " + String(ESP.getFreeHeap()));
}

bool Network::connectAll() {
  Serial.println("Connecting to WiFi: " + config->wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config->wifi_ssid.c_str(), config->wifi_password.c_str());
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) { 
    Serial.println("Failed to connect to WiFi");
    return false;
  }
  Serial.println("WiFi connected, IP: " + WiFi.localIP().toString());

  timeClient->begin();
  if (!timeClient->update()) {
    Serial.println("Failed to connect to NTP");
    return false;
  }
  Serial.println("NTP time updated: " + String(timeClient->getFormattedTime()));

  Serial.println("Connecting to MQTT server: " + config->mqtt_server);
  bool connected = mqttClient.connect(
    config->device_id.c_str(),
    config->mqtt_username.c_str(),
    config->mqtt_password.c_str(),
    "press/status",
    2,
    true,
    "offline",
    true
  );

  if (!connected) { 
    Serial.println("Failed to connect to MQTT: " + String(mqttClient.state()));
    return false;
  }
  Serial.println("MQTT connected");

  mqttClient.subscribe(config->mqtt_turnon_topic.c_str());
  Serial.println("Subscribed to MQTT topic: " + config->mqtt_turnon_topic);
  return true;
}

bool Network::sendDeviceInfo() {
  if (isConnected()) {
    DynamicJsonDocument doc(256);
    doc["signal_quality"] = WiFi.RSSI();
    doc["time"] = getTime();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["device_id"] = config->device_id;
    doc["counting"] = config->counting;
    String json;
    serializeJson(doc, json);
    
    Serial.print("Publishing device info to topic: ");
    Serial.println(config->mqtt_device_info_topic);
    Serial.print("Device info message: ");
    Serial.println(json);
    bool success = mqttClient.publish(config->mqtt_device_info_topic.c_str(), json.c_str(), true);
    if (success) {
      Serial.println("Device info published successfully");
      return true;
    } else {
      Serial.println("Failed to publish device info after retries");
      return false;
    }
  }else{
    return false;
  }
}

unsigned long Network::getTime() {
  return timeClient->getEpochTime();
}

void Network::loop() {
  mqttClient.loop();
  timeClient->update();
  unsigned long now = getTime();
  // check every 60 seconds
  if (now - currentTime >= 15) {
    Serial.print("Checking network:");
    currentTime = now;
    if (!isConnected()) {
      Serial.print("mqtt disconnected retrying");
      bool connected = false;
      connected = mqttClient.connect(config->device_id.c_str(), config->mqtt_username.c_str(),config->mqtt_password.c_str(),"press/status",2,true,"offline",true);
      Serial.print(".");
      Serial.println("connected");
      if (connected) {
        mqttClient.publish("press/status", "online", true); // New: Publish online on reconnect
        mqttClient.subscribe(config->mqtt_turnon_topic.c_str()); // New: Resubscribe on reconnect
        Serial.println("Reconnected and resubscribed to MQTT topic: " + config->mqtt_turnon_topic);
      } else {
        Serial.println("Reconnect failed, state: " + String(mqttClient.state()));
      }
    }else{
      Serial.println(" Success");
    }
  }
}

bool Network::isConnected() {
  bool MQTTconnected = mqttClient.connected();
  bool WIFIconnected = WiFi.status() == WL_CONNECTED;
  if (!WIFIconnected){
    Serial.println("Network not connected WiFi=" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + ". Free heap: " + String(ESP.getFreeHeap()));
    WiFi.begin(config->wifi_ssid.c_str(), config->wifi_password.c_str());
    return false;
  }else if (!MQTTconnected){
     Serial.println("Network not connected MQTT=" + String(mqttClient.connected() == true ? "connected" : "disconnected") + ". Free heap: " + String(ESP.getFreeHeap()));
     return false;
  }
  return true;
}

PubSubClient& Network::getMqttClient() {
  return mqttClient;
}

void Network::sendCountSignal(int press_count){
  extern String current_die;
  if (!isConnected()) {
    Serial.println("MQTT not connected, skipping button signal");
    return;
  }
  DynamicJsonDocument doc(256);
  doc["device_id"] = config->device_id;
  doc["timestamp"] = getTime();
  if (current_die == "") {
    doc["die_id"] = JsonVariant();
  } else {
    doc["die_id"] = current_die;
  }
  doc["count"] = press_count;

  String json;
  serializeJson(doc, json);
  Serial.print("Device info message sent to topic: ");
  Serial.print(config->mqtt_counting_topic.c_str());
  Serial.print("message: ");
  Serial.println(json.c_str());
  mqttClient.publish(config->mqtt_counting_topic.c_str(), json.c_str());
}
void Network::sendButtonSignal(const String& signal) {
  if (!isConnected()) {
    Serial.println("MQTT not connected, skipping button signal");
    return;
  }
  DynamicJsonDocument doc(128);
  doc["device_id"] = config->device_id;
  doc["signal"] = signal;

  String json;
  serializeJson(doc, json);
  Serial.print("Publishing button signal to topic: ");
  Serial.println(config->mqtt_button_topic);
  if (mqttClient.publish(config->mqtt_button_topic.c_str(), json.c_str())) {
    Serial.println("Button signal published successfully");
  } else {
    Serial.println("Failed to publish button signal");
  }
}
//void Network::startupCounting();

void Network::callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("MQTT callback received. Free heap: " + String(ESP.getFreeHeap()));
  payload[length] = '\0';
  String message = String((char*)payload);

  DynamicJsonDocument doc(128);
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println("Failed to parse MQTT callback message: " + String(error.c_str()));
    return;
  }

  String dev = doc["device"].as<String>();
  if (dev != instance->config->device_id) {
    Serial.println("Message ignored: device ID mismatch (received: " + dev + ", expected: " + instance->config->device_id + ")");
    return;
  }

  int rel = doc["relay"];
  setRelay(rel == 1);
  Serial.print("Relay set to: ");
  Serial.println(rel == 1 ? "ON" : "OFF");

  if (rel == 1) {
    pressCounter.startCounting();
    Serial.println("Started counting");
  } else {
    pressCounter.stopCounting();
    Serial.println("Stopped counting");
  }
  Serial.println("MQTT callback completed. Free heap: " + String(ESP.getFreeHeap()));
}