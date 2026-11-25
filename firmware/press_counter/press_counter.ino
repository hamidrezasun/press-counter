// press_counter.ino - hydraulic press counter, v6 board: ONE ESP32-WROOM-32 does everything.
//
//   * counts press strokes from the opto-isolated 230 VAC sense input (interrupt time-stamped, debounced),
//   * switches the press-contactor relay (personnel card toggles it; idle timeout; over-temperature stop),
//   * reads / writes NFC cards (RC522: personnel:<id>, die:<id>), DS18B20 temperature, two call buttons, buzzer, alert LED,
//   * shows the counter on a 0.96in I2C OLED,
//   * WiFi + NTP + MQTT (the only data backend) + web page / JSON API + firmware updates (USB, ArduinoOTA, own update server).
// It keeps counting and switching without any network.   See docs/FIRMWARE.md and docs/PROTOCOL.md.
//
// Build: Arduino IDE / arduino-cli, board "ESP32 Dev Module" (esp32:esp32), Partition scheme "Minimal SPIFFS (1.9MB APP with OTA)",
// libraries MFRC522, PubSubClient, ArduinoJson 6.  Upload over the USB port of the board (CH340G with auto-reset: no buttons).
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "pc_config.h"
#include "pc_buttons.h"
#include "pc_display.h"
#include "pc_errors.h"
#include "pc_events.h"
#include "pc_io.h"
#include "pc_log.h"
#include "pc_menu.h"
#include "pc_mqtt.h"
#include "pc_net.h"
#include "pc_nfc.h"
#include "pc_outputs.h"
#include "pc_press.h"
#include "pc_sense.h"
#include "pc_session.h"
#include "pc_shell.h"
#include "pc_store.h"
#include "pc_temp.h"
#include "pc_update.h"
#include "pc_web.h"

#ifdef PC_BENCH_DEBUG
#include <esp_rom_sys.h>
#define PC_DBG(s) do { ets_printf("DBG %s\n", s); } while (0)
#else
#define PC_DBG(s) do {} while (0)
#endif

namespace {
uint32_t g_tempAt = 0, g_statusAt = 0, g_infoAt = 0, g_flushAt = 0;

void onTag(char kind, const char* text) {
  session::handleTag(kind, text);
  ui::wake();
}

void onStroke(uint32_t durationMs) {
  session::handleStroke(durationMs);
  ui::wake();
}

const char* resetText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brown-out";
    case ESP_RST_EXT: return "reset pin";
    default: return "other";
  }
}
}  // namespace

void setup() {
  PC_DBG("setup start");
  io::begin();                                              // relay OFF first, whatever happens next
  PC_DBG("io ok");
  outputs::begin();                                         // output relays OFF as well
  PC_DBG("outputs ok");
  shell::begin();
  PC_DBG("shell ok");
  const bool settingsOk = store::begin();
  PC_DBG("store ok");
  err::begin();
  PC_DBG("err ok");
  events::begin();
  PC_DBG("events ok");
  const esp_reset_reason_t rr = esp_reset_reason();
  plog::log("press counter %s starting, reset: %s", PC_FW_VERSION_STR, resetText(rr));
  if (!settingsOk) { err::latch(pc::ERR_SETTINGS); store::save(); }
  if (rr == ESP_RST_PANIC || rr == ESP_RST_INT_WDT || rr == ESP_RST_TASK_WDT || rr == ESP_RST_WDT) err::latch(pc::ERR_WATCHDOG, (uint16_t)rr);

  buttons::begin();
  PC_DBG("buttons ok");
  ui::begin();
  PC_DBG("ui ok");
  temp::begin();
  PC_DBG("temp ok");
  sense::begin();                                           // 12/24 V and AUX inputs (ADC1)
  PC_DBG("sense ok");
  press::begin();
  PC_DBG("press ok");
  press::onStroke(onStroke);
  session::begin();
  PC_DBG("session ok");
  nfc::onTag(onTag);
  const bool rfid = nfc::begin();
  PC_DBG("nfc ok");
  plog::log("RC522 reader %s", rfid ? "ok" : "MISSING (E01)");

  net::begin();
  PC_DBG("net ok");
  mqtt::begin();
  PC_DBG("mqtt ok");
  web::begin();
  PC_DBG("web ok");
  fwupd::begin();
  PC_DBG("fwupd ok");

  // task watchdog on the loop: a hang restarts the board (the relay drops = machine stops, which is the safe state)
  esp_task_wdt_config_t wdt = {};
  wdt.timeout_ms = WDT_TIMEOUT_S * 1000;
  wdt.idle_core_mask = 0;
  wdt.trigger_panic = true;
  esp_task_wdt_reconfigure(&wdt);
  esp_task_wdt_add(nullptr);

  io::play(io::SND_STARTUP);
  g_tempAt = g_statusAt = millis();
}

void loop() {
  // A firmware update erases/writes flash with the cache disabled, so the loop task can be starved
  // for far longer than the normal watchdog window.  Widen it while an update is in progress.
  static bool updWdt = false;
  const bool busy = fwupd::busy();
  if (busy != updWdt) {
    updWdt = busy;
    esp_task_wdt_config_t w = {};
    w.timeout_ms = busy ? 180000UL : (uint32_t)WDT_TIMEOUT_S * 1000UL;
    w.idle_core_mask = 0;
    w.trigger_panic = true;
    esp_task_wdt_reconfigure(&w);
  }
  esp_task_wdt_reset();
  press::poll();                                           // first: stroke timing never waits for anything else
  buttons::poll();
  nfc::poll();
  temp::poll();
  sense::poll();
  session::poll();
  outputs::poll();
  err::poll();
  io::poll();
  ui::poll();
  shell::poll();
  menu::poll();                                            // two-button menu owns the display while open
  net::loop();
  // While an update is downloading/flashing it owns the lwIP stack: the MQTT publisher, the web
  // server and the event queue are suspended, otherwise two cores touch the same pbufs and lwIP
  // asserts (pbuf_free: p->ref > 0).  Local control (press, buttons, display, relay) keeps running.
  if (!fwupd::busy()) {
    mqtt::loop();
    events::poll();
    web::loop();
  }
  fwupd::loop();
  if (buttons::takeSetupRequest()) menu::open();            // both call buttons: local menu (also WiFi setup)

  const uint32_t now = millis();
  if (now - g_tempAt >= TEMP_REPORT_MS) {
    g_tempAt = now;
    if (temp::valid()) events::pushTemp(temp::tempC10());
  }
  if (now - g_statusAt >= STATUS_PERIOD_MS) { g_statusAt = now; mqtt::publishStatus(); }
  if (mqtt::connected() && now - g_infoAt >= INFO_PERIOD_MS) { g_infoAt = now; mqtt::publishInfo(); }
  // persist the lifetime counter shortly after the last stroke, never while a stroke is running
  if (!press::active() && now - g_flushAt > 10000UL) { g_flushAt = now; store::lifetimeFlush(false); }
}
