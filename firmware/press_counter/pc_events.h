// pc_events.h - reliable event queue.
// Every press, tag, relay change, call, error and temperature report becomes an Event.  Events are published on MQTT
// (<prefix>/<device>/event/<kind>) and stay in the queue (RAM) while the broker is not reachable.
#ifndef PC_EVENTS_H
#define PC_EVENTS_H

#include "pc_config.h"

namespace events {

struct Event {
  char     type;            // 'P' press, 'N' nfc, 'R' relay, 'O' output relay, 'C' call, 'E' error, 'T' temperature
  uint16_t seq;
  uint64_t tms;             // unix ms of the event (0 = clock not set yet: derived from atMs when it is published)
  uint32_t atMs;            // millis() when it happened
  uint32_t a, b;            // meaning depends on type, see toJson()
  int32_t  c;
  uint16_t d;
  char     text[MAX_TAG_TEXT + 2];
  uint32_t order;           // queue order
  bool     used;
};

void begin();
void poll();                                           // publishes queued events (a few per call)

void pushPress(uint32_t count, uint16_t durMs, int16_t tempC10, bool relayOn);
void pushNfc(char kind, bool relayAfter, const char* text);
void pushRelay(bool on, uint8_t reason);
void pushOutput(uint8_t index, bool on, uint8_t reason);   // OUT1..OUT4 (index 1..4)
void pushCall(uint8_t type, bool raised);
void pushError(uint8_t code, bool raised, uint16_t detail);
void pushTemp(int16_t tempC10);

uint8_t pending();                                     // events waiting for delivery
uint32_t dropped();                                    // events lost because the queue was full
uint32_t published();
// MQTT sub-topic ("event/press" ...) and JSON payload of an event
bool toJson(const Event& e, const char*& kind, char* out, size_t outSize);

}  // namespace events

#endif
