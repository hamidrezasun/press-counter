// Press counter - machine controller (STM32F103C8T6 on the v5 MAIN board).
//
// Counts press strokes from the opto-isolated sense input, switches the machine relay, reads RFID cards, drives the
// display / buttons / buzzer, offers a USB console, and hands every event to the ESP8266 gateway.  It keeps working
// without any network.   See docs/FIRMWARE.md and docs/PROTOCOL.md.
#include <Arduino.h>
#include "config.h"
#include "store.h"
#include "clock.h"
#include "errors.h"
#include "events.h"
#include "io.h"
#include "press.h"
#include "temp.h"
#include "buttons.h"
#include "nfc.h"
#include "session.h"
#include "link.h"
#include "ui.h"
#include "outputs.h"
#include "sense.h"
#include "shell.h"
#include "bootloader.h"

namespace {
uint32_t g_lastTempReport = 0;

void wdtStart(uint32_t ms) {                  // independent watchdog: LSI ~40 kHz / 64 = 625 Hz
  IWDG->KR = 0x5555;
  IWDG->PR = 4;
  IWDG->RLR = (uint16_t)(ms * 625UL / 1000UL > 0xFFF ? 0xFFF : ms * 625UL / 1000UL);
  uint32_t guard = 100000UL;
  while (IWDG->SR && --guard) {}
  IWDG->KR = 0xAAAA;
  IWDG->KR = 0xCCCC;
}
inline void wdtKick() { IWDG->KR = 0xAAAA; }

void onTag(char kind, const char* text) {
  session::handleTag(kind, text);
  ui::wake();
}
}  // namespace

void setup() {
  bootloader::earlyInit();                                  // reset cause + a pending request to jump into the ROM bootloader
  // PB3 / PB4 / PA15 are button inputs: free them from JTAG, keep the SWD debug port
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

  io::begin();                                              // relay OFF first, whatever happens next
  outputs::begin();                                         // output relays OFF as well
  const bool settingsOk = store::load();
  err::begin();
  events::begin();
  if (!settingsOk) err::latch(pc::ERR_SETTINGS);
  if (bootloader::wasWatchdogReset()) err::latch(pc::ERR_WATCHDOG);

  buttons::begin();
  ui::begin();
  ui::bootMessage("display ok");

  temp::begin();
  sense::begin();                                           // 12 V / 230 V measurement (ADC2 + TIM4)
  press::begin();
  press::onStroke(session::handleStroke);
  session::begin();
  ui::bootMessage("sense input ok");

  nfc::onTag(onTag);
  const bool rfid = nfc::begin();
  ui::bootMessage(rfid ? "RFID reader ok" : "RFID reader MISSING (E01)");

  link::begin();
  ui::bootMessage("gateway link started");

  shell::begin();                                           // USB serial console (works with or without a host)

  io::play(io::SND_STARTUP);
  delay(400);
  wdtStart(IWDG_TIMEOUT_MS);
  g_lastTempReport = millis();
}

void loop() {
  wdtKick();
  press::poll();                                           // first: stroke timing must never wait for anything else
  buttons::poll();
  nfc::poll();
  temp::poll();
  session::poll();
  outputs::poll();
  sense::poll();
  link::poll();
  events::poll();
  err::poll();
  io::poll();
  ui::poll();
  shell::poll();

  const uint32_t now = millis();
  if (now - g_lastTempReport >= TEMP_REPORT_MS) {
    g_lastTempReport = now;
    if (temp::valid()) events::pushTemp(temp::tempC10());
    store::lifetimeFlush(false);
  }
  // persist the lifetime counter shortly after the last stroke, never while a stroke is running
  static uint32_t lastFlush = 0;
  if (!press::active() && now - lastFlush > 10000UL) { lastFlush = now; store::lifetimeFlush(false); }
}
