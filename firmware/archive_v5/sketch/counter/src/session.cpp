#include "session.h"
#include "store.h"
#include "io.h"
#include "events.h"
#include "errors.h"
#include "press.h"
#include "temp.h"
#include "nfc.h"
#include "link.h"

namespace {
bool     g_relay = false;
uint32_t g_relayOnAt = 0;
uint32_t g_relayOffAt = 0;
uint32_t g_lastActivity = 0;                    // relay-on time or last stroke: the idle timeout counts from here
char     g_operator[MAX_TAG_TEXT + 2] = "";
char     g_die[MAX_TAG_TEXT + 2] = "";
uint32_t g_callAt[3] = {0, 0, 0};               // millis() when raised, 0 = not active
char     g_banner[32] = "";
uint32_t g_bannerUntil = 0;

const char* afterColon(const char* t) {
  const char* c = strchr(t, ':');
  return c ? c + 1 : t;
}
}  // namespace

namespace session {

void begin() {
  g_relay = false;
  g_relayOnAt = g_relayOffAt = 0;
  g_operator[0] = g_die[0] = '\0';
}

bool relayOn() { return g_relay; }
bool canEnable() { return !err::active(pc::ERR_OVERTEMP); }
uint32_t sessionSeconds() { return g_relay ? (millis() - g_relayOnAt) / 1000UL : 0; }
const char* operatorId() { return g_operator; }
const char* dieId() { return g_die; }

void banner(const char* text, uint32_t ms) {
  pc::copyStr(g_banner, text, sizeof(g_banner));
  g_bannerUntil = millis() + ms;
}
const char* bannerText() { return (g_banner[0] && (int32_t)(millis() - g_bannerUntil) < 0) ? g_banner : ""; }

bool setRelay(bool on, uint8_t reason) {
  if (on && !canEnable()) { banner("E04 OVER TEMP"); io::play(io::SND_ERROR); return false; }
  if (on == g_relay) return true;
  g_relay = on;
  io::setRelayPin(on);
  if (on) { g_relayOnAt = millis(); g_lastActivity = g_relayOnAt; }
  else {
    g_relayOffAt = millis();
    g_operator[0] = g_die[0] = '\0';
  }
  events::pushRelay(on, reason);
  return true;
}

void toggleRelay(uint8_t reason) { setRelay(!g_relay, reason); }

void resetCounter() {
  press::resetCount();
}

void handleStroke(uint32_t durationMs) {
  g_lastActivity = millis();
  events::pushPress(press::count(), durationMs > 65535UL ? 65535 : (uint16_t)durationMs, temp::tempC10() == INT16_MIN ? 0 : temp::tempC10(), g_relay);
  io::play(io::SND_SUCCESS);
  // a stroke while the machine is disabled (and not just after switching it off) means the contactor is fed some other way
  if (!g_relay && g_relayOffAt && millis() - g_relayOffAt > 2000UL) err::latch(pc::ERR_BYPASS, (uint16_t)durationMs);
}

void handleTag(char kind, const char* text) {
  if (kind == pc::NFC_WRITTEN) {
    io::play(io::SND_SUCCESS);
    banner("TAG WRITTEN");
    events::pushNfc(pc::NFC_WRITTEN, g_relay, text);
    return;
  }
  if (kind == pc::NFC_PERSONNEL) {
    const bool wasOn = g_relay;
    const bool ok = setRelay(!wasOn, pc::RR_TAG);
    if (ok) {
      press::resetCount();                               // a new session starts / the old one ends: counter back to zero
      if (g_relay) pc::copyStr(g_operator, afterColon(text), sizeof(g_operator));
      io::play(io::SND_SUCCESS);
      banner(g_relay ? "MACHINE ENABLED" : "MACHINE STOPPED");
    }
  } else if (kind == pc::NFC_DIE) {
    pc::copyStr(g_die, afterColon(text), sizeof(g_die));
    io::play(io::SND_SUCCESS);
    banner("DIE RECORDED");
  } else {
    io::play(io::SND_WARNING);                           // unknown format / unreadable card: log it, change nothing
    banner(text[0] ? "UNKNOWN CARD" : "CARD NOT READABLE");
    if (!text[0]) return;
  }
  events::pushNfc(kind, g_relay, text);                  // relay state AFTER handling: the analyzer starts / ends sessions from this
}

void call(uint8_t type) {
  if (type < 1 || type > 2 || g_callAt[type]) return;
  g_callAt[type] = millis() ? millis() : 1;
  events::pushCall(type, true);
  io::play(io::SND_CALL);
  banner(type == pc::CALL_SUPERVISOR ? "SUPERVISOR CALLED" : "NET / MAINT CALLED");
}

void cancelCall(uint8_t type) {
  if (type < 1 || type > 2 || !g_callAt[type]) return;
  g_callAt[type] = 0;
  events::pushCall(type, false);
  banner("CALL CANCELLED");
}

void ackCall(uint8_t type) {
  for (uint8_t t = 1; t <= 2; t++) {
    if ((type == 0 || type == t) && g_callAt[t]) {
      g_callAt[t] = 0;
      events::pushCall(t, false);
      banner("CALL ACKNOWLEDGED");
      io::play(io::SND_SUCCESS);
    }
  }
}

uint8_t callMask() { return (uint8_t)((g_callAt[1] ? 2 : 0) | (g_callAt[2] ? 4 : 0)); }
uint32_t callAgeMs(uint8_t type) { return (type >= 1 && type <= 2 && g_callAt[type]) ? millis() - g_callAt[type] : 0; }

void remoteCommand(uint8_t cmd, uint8_t arg, uint8_t from) {
  switch (cmd) {
    case pc::CMD_RELAY_OFF: setRelay(false, from); break;                          // stopping is always allowed
    case pc::CMD_RELAY_ON:
      if (store::cfg.remoteStart) setRelay(true, from);
      else { banner("REMOTE START OFF"); io::play(io::SND_WARNING); }
      break;
    case pc::CMD_RESET_CNT: press::resetCount(); break;
    case pc::CMD_ACK_CALL: ackCall(arg); break;
    case pc::CMD_BUZZ: io::play(arg == 0 ? io::SND_SUCCESS : arg == 1 ? io::SND_WARNING : io::SND_ERROR); break;
    case pc::CMD_CLEAR_ERR: err::acknowledge(); break;
    case pc::CMD_CALL: call(arg); break;
    case pc::CMD_REBOOT: NVIC_SystemReset(); break;
    default: break;
  }
}

void poll() {
  const uint32_t now = millis();

  // safety: over-temperature switches the machine off
  if (g_relay && err::active(pc::ERR_OVERTEMP)) setRelay(false, pc::RR_ERROR);

  // idle timeout: no stroke for idleMin minutes after the machine was enabled
  if (g_relay && store::cfg.idleMin > 0 && now - g_lastActivity > (uint32_t)store::cfg.idleMin * 60000UL) {
    setRelay(false, pc::RR_IDLE);
    io::play(io::SND_WARNING);
    banner("IDLE: MACHINE OFF", 6000);
  }

  // sense input stuck active (contactor welded / wiring)
  err::set(pc::ERR_SENSE_STUCK, press::activeForMs() > (uint32_t)store::cfg.stuckSec * 1000UL);

  // LEDs: green = machine enabled; red = errors (fast) / warnings and calls (slow)
  io::Alert alert = io::ALERT_OFF;
  const uint8_t top = err::top();
  if (top) alert = pc::errorSeverity(top) >= 2 ? io::ALERT_FAST : io::ALERT_SLOW;
  else if (callMask()) alert = io::ALERT_SLOW;
  io::setLeds(g_relay, alert);
}

}  // namespace session
