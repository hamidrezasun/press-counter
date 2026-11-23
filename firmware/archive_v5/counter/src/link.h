// link.h - UART link to the ESP8266 gateway (USART1, 38400 8N1, ASCII lines "$payload*HH\n", see docs/PROTOCOL.md).
#ifndef LINK_H
#define LINK_H

#include "config.h"

#define LinkSerial Serial1            // needs -DENABLE_HWSERIAL1.  USART1: PA9 TX -> ESP GPIO13 (RX after swap), PA10 RX <- ESP GPIO15 (TX after swap)

namespace link {
void begin();
void poll();                          // receive + parse + transmit + periodic status / identity
bool send(const char* payload);       // queue a frame; false if the TX buffer is full (retry next loop)

bool alive();                         // the ESP answered within LINK_ALIVE_MS
bool wifiUp();                        // ... and reports WiFi connected
bool serverUp();                      // ... and the last server write worked
bool ntpOk();
bool gatewayReady();                  // alive && wifiUp: events can be handed to the ESP
int16_t rssi();
const char* ip();
uint8_t fwState();                    // 0 idle, 1 checking, 2 downloading, 3 flashing, 4 failed, 5 done (firmware update progress)
uint8_t fwPercent();
uint32_t lastRxMs();
uint32_t badFrames();

void sendIdentity();
void sendStatus();
void sendSetting(const char* key);    // "Q,key,value"
void sendAllSettings();
}  // namespace link

#endif
