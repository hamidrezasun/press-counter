// ui.h - TFT user interface: status pages, menu, settings editor, calls, error list.
//   LEFT / RIGHT : previous / next page          UP / DOWN : scroll inside a page
//   OK           : open the menu                 CALL SUPERVISOR / CALL NET buttons: short = call, hold 1.5 s = cancel
#ifndef UI_H
#define UI_H

#include "config.h"

namespace ui {
void begin();
void poll();                          // call every loop: buttons -> actions, redraw when something changed
void bootMessage(const char* line);   // progress text during start-up
void wake();                          // restore the back-light (tag read, error raised)
}  // namespace ui

#endif
