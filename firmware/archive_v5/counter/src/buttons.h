// buttons.h - the seven panel buttons (D-pad cross + OK + two call buttons): debouncing, short / long press, auto-repeat.
#ifndef BUTTONS_H
#define BUTTONS_H

#include "config.h"

namespace buttons {
enum Btn : uint8_t { UP, DOWN, LEFT, RIGHT, OK, SUP, NET, COUNT };
enum Kind : uint8_t {
  PRESS,        // the moment the button goes down
  SHORT,        // released before BTN_LONG_MS
  LONG,         // held for BTN_LONG_MS (sent once)
  REPEAT        // auto-repeat of UP / DOWN / LEFT / RIGHT while held (after the long-press time)
};
struct Event { Btn button; Kind kind; };

void begin();
void poll();
bool pop(Event& e);                    // oldest pending event
bool down(Btn b);
uint32_t heldMs(Btn b);
uint32_t lastActivityMs();             // millis() of the last press / release (display dimming)
const char* name(Btn b);
}  // namespace buttons

#endif
