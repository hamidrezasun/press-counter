// io.h - outputs: machine relay, buzzer patterns, LEDs, display back-light.
#ifndef IO_H
#define IO_H

#include "config.h"

namespace io {
enum Sound : uint8_t { SND_CLICK, SND_SUCCESS, SND_STARTUP, SND_CALL, SND_WARNING, SND_ERROR };    // ascending priority
enum Alert : uint8_t { ALERT_OFF, ALERT_STEADY, ALERT_SLOW, ALERT_FAST };

void begin();                              // outputs to their safe level: relay OFF, buzzer silent
void poll();                               // buzzer pattern engine + LED blinking
void setRelayPin(bool on);
bool relayPin();
void play(Sound s);                        // honours settings.sound
void stopSound();
bool buzzerOn();
void setLeds(bool run, Alert alert);       // green = machine enabled, red = alert pattern
void setBacklight(uint8_t percent);
}  // namespace io

#endif
