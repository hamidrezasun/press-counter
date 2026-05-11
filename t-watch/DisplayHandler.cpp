#include "DisplayHandler.h"
#include <freertos/semphr.h>

extern SemaphoreHandle_t lastMessageTimeMutex;

void DisplayHandler::init(TTGOClass* t) {
  twatch = t;
  lastMessageTime = millis();
  displayOn = true;

  // Initialize TFT
  twatch->tft->fillScreen(TFT_BLACK);
  twatch->tft->setTextColor(TFT_WHITE, TFT_BLACK);
  twatch->tft->setTextFont(4);
}

void DisplayHandler::showMessage(const char* msg) {
  if (xSemaphoreTake(lastMessageTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    twatch->tft->fillScreen(TFT_BLACK);
    twatch->tft->setTextColor(TFT_WHITE, TFT_BLACK);
    twatch->tft->setTextFont(4);
    // Center text on 240x240 screen
    int16_t textWidth = twatch->tft->textWidth(msg);
    int16_t x = (240 - textWidth) / 2;
    int16_t y = 120 - 16; // Approximate center for font 4
    twatch->tft->drawString(msg, x > 0 ? x : 0, y);
    lastMessageTime = millis();
    Serial.println("Displayed: " + String(msg));
    xSemaphoreGive(lastMessageTimeMutex);
  }
}

void DisplayHandler::setDisplayOn(bool on) {
  if (displayOn != on) {
    displayOn = on;
    if (on) {
      twatch->openBL();
    } else {
      twatch->closeBL();
    }
    Serial.println(displayOn ? "Display ON" : "Display OFF");
  }
}

unsigned long DisplayHandler::getLastMessageTime() {
  unsigned long time;
  if (xSemaphoreTake(lastMessageTimeMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    time = lastMessageTime;
    xSemaphoreGive(lastMessageTimeMutex);
  } else {
    time = millis();
  }
  return time;
}