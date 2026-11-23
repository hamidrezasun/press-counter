// pc_io.h - outputs: machine relay, buzzer patterns, red alert LED.
#ifndef PC_IO_H
#define PC_IO_H

#include "pc_config.h"

namespace io {
enum Sound : uint8_t { SND_CLICK, SND_SUCCESS, SND_STARTUP, SND_CALL, SND_WARNING, SND_ERROR };    // ascending priority
enum Alert : uint8_t { ALERT_OFF, ALERT_STEADY, ALERT_SLOW, ALERT_FAST };

void outputLow(uint8_t pin);               // make `pin` an output that is LOW from the first instant (relay drivers)
void begin();                              // outputs to their safe level: relay OFF, buzzer silent
void poll();                               // buzzer pattern engine + LED blinking
void setRelayPin(bool on);
bool relayPin();
void play(Sound s);                        // honours settings.sound
void stopSound();
void setAlert(Alert alert);
bool alertLit();                           // the red LED is on right now (web page mirror)
}  // namespace io

#endif
