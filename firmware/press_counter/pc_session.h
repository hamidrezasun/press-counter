// pc_session.h - machine enable relay, operator / die identification, calls and the idle / safety rules.
#ifndef PC_SESSION_H
#define PC_SESSION_H

#include "pc_config.h"

namespace session {
void begin();
void poll();

// machine relay (K1).  Always OFF after a reset: the machine is only enabled by a personnel card or by the menu.
bool relayOn();
bool canEnable();                                       // false while a condition forbids starting (E04 over-temperature)
bool setRelay(bool on, uint8_t reason);                 // returns true if the relay is in the requested state afterwards
void toggleRelay(uint8_t reason);
uint32_t sessionSeconds();                              // seconds since the relay was switched on (0 when off)
const char* operatorId();
const char* dieId();
bool dieTest();                                         // true while a die card has the machine on for a try-out (strokes are not counted)

void handleTag(char kind, const char* text);            // from nfc::onTag
void handleStroke(uint32_t durationMs);                 // from press::onStroke
void resetCounter();

// calls (CALL SUPERVISOR = 1, CALL NET / MAINTENANCE = 2)
void call(uint8_t type);
void cancelCall(uint8_t type);                          // by the operator (long press)
void ackCall(uint8_t type);                             // type 0 = all; by the supervisor (web / MQTT)
uint8_t callMask();                                     // bit n set = call type n pending
uint32_t callAgeMs(uint8_t type);

// commands from the web page / MQTT / USB console; `from` is a pc::RelayReason
void remoteCommand(uint8_t cmd, uint8_t arg, uint8_t from);   // cmd = pc::Cmd

// short message line for the display ("TAG WRITTEN", "SUPERVISOR CALLED" ...)
void banner(const char* text, uint32_t ms = 4000);
const char* bannerText();                               // "" when expired
}  // namespace session

#endif
