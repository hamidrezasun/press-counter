#include <LilyGoWatch.h>
#include "config.h"
#include "ConfigManager.h"
#include "WebServer.h"
#include "MQTTHandler.h"
#include "DisplayHandler.h"
#include "HardwareHandler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

TTGOClass *twatch = nullptr;
ConfigManager configManager;
WebServer webServer;
MQTTHandler mqttHandler;
DisplayHandler displayHandler;
HardwareHandler hardwareHandler;

// Global pointers for callback access
ConfigManager* g_config = &configManager;
DisplayHandler* g_display = &displayHandler;
HardwareHandler* g_hardware = &hardwareHandler;

// Task handles
TaskHandle_t touchTaskHandle;
TaskHandle_t mqttTaskHandle;
TaskHandle_t displayTimeoutTaskHandle;

// Mutex for last message time
SemaphoreHandle_t lastMessageTimeMutex;

void touchTask(void *pvParameters) {
  for (;;) {
    uint16_t x, y;
    if (twatch->getTouch(&x, &y)) {
      g_display->setDisplayOn(true);
      Serial.printf("Touch at %d, %d\n", x, y);
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // Check touch every 50ms
  }
}

void mqttTask(void *pvParameters) {
  for (;;) {
    mqttHandler.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void displayTimeoutTask(void *pvParameters) {
  Config cfg = g_config->getConfig();
  for (;;) {
    if (cfg.displayTimeout > 0) {
      unsigned long lastMessageTime = g_display->getLastMessageTime();
      if (millis() - lastMessageTime > cfg.displayTimeout * 1000UL) {
        g_display->setDisplayOn(false);
      }
    }
    cfg = g_config->getConfig();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting LILYGO T-Watch MQTT Project with RTOS");

  // Initialize hardware
  twatch = TTGOClass::getWatch();
  twatch->begin();
  twatch->openBL();

  // Create mutex
  lastMessageTimeMutex = xSemaphoreCreateMutex();

  // Initialize handlers
  configManager.init();
  displayHandler.init(twatch);
  hardwareHandler.init();
  mqttHandler.init(&configManager, &displayHandler, &hardwareHandler);
  webServer.init(&configManager, &mqttHandler, &displayHandler, &hardwareHandler);

  // Show boot screen
  displayHandler.showMessage("Booted! Connect to WiFi for config.");
  Serial.println("Setup complete");

  // Create tasks
  xTaskCreatePinnedToCore(
    touchTask,         // Task function
    "Touch Task",      // Name
    2048,              // Stack size
    NULL,              // Parameters
    2,                 // Priority
    &touchTaskHandle,  // Handle
    1                  // Core 1
  );

  xTaskCreatePinnedToCore(
    mqttTask,         // Task function
    "MQTT Task",      // Name
    4096,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    &mqttTaskHandle,  // Handle
    0                 // Core 0
  );

  xTaskCreatePinnedToCore(
    displayTimeoutTask,  // Task function
    "Display Timeout Task", // Name
    2048,             // Stack size
    NULL,             // Parameters
    1,                // Priority
    &displayTimeoutTaskHandle, // Handle
    0                 // Core 0
  );
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));  // Idle to save power
}