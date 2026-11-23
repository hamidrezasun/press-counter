// events.h - reliable event queue.
// Every press, tag, relay change, call and error becomes an Event.  Events are handed to the ESP8266 gateway over the UART link
// (when it reports WiFi up) and stay in the queue until the ESP acknowledges them, otherwise they wait.
#ifndef EVENTS_H
#define EVENTS_H

#include "config.h"

namespace events {

struct Event {
  char     type;            // 'P' press, 'N' nfc, 'R' relay, 'C' call, 'E' error, 'T' temperature
  uint16_t seq;             // sequence number (acknowledged by the gateway)
  uint32_t tsec;            // unix time of the event (0 = unknown, the ESP stamps it on arrival)
  uint16_t tms;
  uint8_t  tries;
  uint32_t sentAt;
  uint32_t a, b;            // meaning depends on type, see events.cpp
  int32_t  c;
  uint16_t d;
  char     text[MAX_TAG_TEXT + 2];
  uint32_t order;           // queue order
  bool     used;
  bool     inFlight;
};

void begin();
void poll();

void pushPress(uint32_t count, uint16_t durMs, int16_t tempC10, bool relayOn);
void pushNfc(char kind, bool relayAfter, const char* text);
void pushRelay(bool on, uint8_t reason);
void pushOutput(uint8_t index, bool on, uint8_t reason);   // OUT1..OUT4 (index 1..4)
void pushCall(uint8_t type, bool raised);
void pushError(uint8_t code, bool raised, uint16_t detail);
void pushTemp(int16_t tempC10);

void onAck(uint16_t seq);                              // ESP confirmed delivery
uint8_t pending();                                     // events waiting for delivery
uint32_t dropped();                                    // events lost because the queue was full
bool format(const Event& e, char* out, size_t outSize);   // the UART line payload of an event

}  // namespace events

#endif
