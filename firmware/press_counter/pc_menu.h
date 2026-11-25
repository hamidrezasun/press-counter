// pc_menu.h - local menu on the OLED, driven by the two call buttons.
//
// Holding CALL SUPERVISOR + CALL NET together (BTN_SETUP_MS) opens it.  With the two buttons the
// user can start an update, look at the log, open the WiFi setup network and change every machine
// setting, without any network at all.
#ifndef PC_MENU_H
#define PC_MENU_H

#include <Arduino.h>

namespace menu {
void open();             // enter the menu (also called by the two-button hold)
void close();
bool active();
void poll();             // call from loop(): buttons + drawing while the menu is open
}  // namespace menu

#endif
