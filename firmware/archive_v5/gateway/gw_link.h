// gw_link.h - UART link to the STM32 machine controller ($payload*HH lines, docs/PROTOCOL.md) and the STM32 flashing wires.
#ifndef GW_LINK_H
#define GW_LINK_H

#include <Arduino.h>
#include "gw_config.h"

struct GwEvent {
  char type;               // 'P' press, 'N' nfc, 'R' relay, 'O' output relay, 'C' call, 'E' error, 'T' temperature
  uint16_t seq;
  uint32_t tsec;           // event time from the STM32 (0 = unknown -> stamp on arrival)
  uint16_t tms;
  int32_t a, b, c;         // type dependent, see gw_telemetry.cpp
  uint16_t d;
  char text[48];
};

struct StmStatus {
  bool valid;
  uint32_t at;             // millis() of the last update
  uint8_t relay;
  uint32_t count, lastDur;
  uint8_t inPress;
  int tempC10;             // 9999 = no sensor
  uint8_t topErr;
  char op[40], die[40];
  uint32_t sessSec, uptime;
  uint8_t queued;
  long lastAgoS;           // -1 = no stroke yet
  uint32_t lifetime;
  uint16_t errMask;
  uint8_t calls;
  uint16_t vdda;
  int mcuT10;
  uint8_t outs;            // output relays OUT1..OUT4, bit n-1 = OUTn on
  uint16_t v12mV;          // 12 V input, millivolt
  uint16_t vac10;          // 230 V input RMS, 0.1 V
};

struct StmIdentity { bool valid; char name[24]; char ver[12]; uint8_t flags; };

namespace gwlink {
typedef bool (*EventHandler)(const GwEvent&);        // return true = accepted (the STM32 is acknowledged)

void begin();
void loop();
void onEvent(EventHandler h);
bool takeUpdateRequest();                            // the operator chose "check firmware update" on the STM32 menu

bool alive();                                        // the STM32 spoke recently
const StmStatus& status();
const StmIdentity& identity();
uint32_t eventsReceived();
uint32_t badFrames();

bool send(const char* payload);
void sendNet(const char* ip, bool wifi, bool server, int rssi, bool ntp, uint8_t fwState, uint8_t fwPct);
void sendTime();
void requestSettings();
bool getSetting(const char* key, char* out, size_t n);
bool setSetting(const char* key, const char* value);  // asks the STM32 (it validates and saves)

// firmware update of the STM32 (STM32 ROM bootloader, AN3155).  loop() is paused while these run.
void pause(bool on);
bool enterBootloader();
void leaveBootloader();
bool stmErase(uint32_t addr, uint32_t size);
bool stmWrite(uint32_t addr, const uint8_t* data, size_t len);
bool stmRead(uint32_t addr, uint8_t* out, size_t len);
}  // namespace gwlink

#endif
