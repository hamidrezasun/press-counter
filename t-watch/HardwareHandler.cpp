#include "HardwareHandler.h"
#include <driver/ledc.h>

void HardwareHandler::init() {
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  ledcSetup(0, 2000, 8);
  ledcAttachPin(BUZZER_PIN, 0);
}

void HardwareHandler::toggleVibrate() {
  vibrateState = !vibrateState;
  digitalWrite(MOTOR_PIN, vibrateState ? HIGH : LOW);
  Serial.println(vibrateState ? "Vibrate ON" : "Vibrate OFF");
}

void HardwareHandler::vibrate(uint32_t duration) {
  digitalWrite(MOTOR_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(duration));
  digitalWrite(MOTOR_PIN, LOW);
}

void HardwareHandler::playBeep(uint32_t frequency, uint32_t duration) {
  ledcSetup(0, frequency, 8);
  ledcWrite(0, 128);
  vTaskDelay(pdMS_TO_TICKS(duration));
  ledcWrite(0, 0);
  Serial.println("Beep played");
}