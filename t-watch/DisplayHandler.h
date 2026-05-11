#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

#include <LilyGoWatch.h>

class DisplayHandler {
public:
  void init(TTGOClass* twatch);
  void showMessage(const char* msg);
  void setDisplayOn(bool on);
  unsigned long getLastMessageTime();

private:
  TTGOClass* twatch;
  unsigned long lastMessageTime;
  bool displayOn;
};

#endif