// pc_buttons.h - the two call buttons (CALL SUPERVISOR, CALL NET / MAINTENANCE).
//   press + release  -> raise the call          hold 1.5 s        -> cancel the call
//   hold CALL NET 3 s -> open the local menu (update, log, WiFi setup, settings)
//   Nothing ever needs both buttons at once.
#ifndef PC_BUTTONS_H
#define PC_BUTTONS_H

#include "pc_config.h"

namespace buttons {
void begin();
void poll();
bool takeMenuRequest();                // CALL NET was held for BTN_MENU_MS
void setMenuMode(bool on);             // while on, short/long presses do not raise or cancel calls
uint32_t lastActivityMs();             // millis() of the last press (display dimming)
bool down(uint8_t type);               // 1 = supervisor, 2 = net (web page mirror)
}  // namespace buttons

#endif
