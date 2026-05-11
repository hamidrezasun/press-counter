#include "MQTTHandler.h"
#include <freertos/semphr.h>

extern DisplayHandler* g_display;
extern HardwareHandler* g_hardware;
extern SemaphoreHandle_t lastMessageTimeMutex;

void MQTTHandler::init(ConfigManager* config, DisplayHandler* display, HardwareHandler* hardware) {
  configMgr = config;
  displayHdlr = display;
  hardwareHdlr = hardware;
  client.setCallback(callback);
  connect();
}

void MQTTHandler::loop() {
  if (!client.connected()) {
    connect();
  }
  client.loop();
}

void MQTTHandler::connect() {
  Config cfg = configMgr->getConfig();
  if (WiFi.status() != WL_CONNECTED) return;

  client.setServer(cfg.mqttServer.c_str(), cfg.mqttPort);

  String clientId = "TWatch-" + String(random(0xffff), HEX);
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (cfg.mqttUser.length() > 0) {
      if (client.connect(clientId.c_str(), cfg.mqttUser.c_str(), cfg.mqttPass.c_str())) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" retrying in 5 seconds");
        vTaskDelay(pdMS_TO_TICKS(5000));
      }
    } else {
      if (client.connect(clientId.c_str())) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" retrying in 5 seconds");
        vTaskDelay(pdMS_TO_TICKS(5000));
      }
    }
  }
  client.subscribe(cfg.mqttTopic.c_str());
  Serial.println("MQTT subscribed to topic");
}

void MQTTHandler::callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.println("MQTT message: " + String(message));
  if (xSemaphoreTake(lastMessageTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    g_display->showMessage(message);
    g_display->setDisplayOn(true);
    g_hardware->vibrate(200);
    xSemaphoreGive(lastMessageTimeMutex);
  }
}