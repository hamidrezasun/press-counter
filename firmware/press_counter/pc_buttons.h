// pc_buttons.h - the two call buttons (CALL SUPERVISOR, CALL NET / MAINTENANCE).
//   press + release  -> raise the call          hold 1.5 s        -> cancel the call
//   hold BOTH 6 s    -> open the WiFi setup access point (the display shows its name and address)
#ifndef PC_BUTTONS_H
#define PC_BUTTONS_H

#include "pc_config.h"

namespace buttons {
void begin();
void poll();
bool takeSetupRequest();               // both buttons were held for BTN_SETUP_MS
uint32_t lastActivityMs();             // millis() of the last press (display dimming)
bool down(uint8_t type);               // 1 = supervisor, 2 = net (web page mirror)
}  // namespace buttons

#endif
