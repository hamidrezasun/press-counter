#include "pc_events.h"
#include "pc_session.h"
#include "pc_errors.h"
#include "pc_log.h"
#include "pc_mqtt.h"

namespace events {
namespace {
Event    g_q[EVENT_QUEUE_SIZE];
uint32_t g_order = 0;
uint16_t g_seq = 0;
uint32_t g_dropped = 0, g_published = 0;
uint32_t g_lastTry = 0;

Event* alloc() {
  for (int i = 0; i < EVENT_QUEUE_SIZE; i++)
    if (!g_q[i].used) return &g_q[i];
  // full: sacrifice the least valuable entry - oldest temperature report, else the oldest non-press event, else the oldest
  Event* victim = nullptr;
  for (int pass = 0; pass < 3 && !victim; pass++) {
    for (int i = 0; i < EVENT_QUEUE_SIZE; i++) {
      Event& e = g_q[i];
      const bool match = pass == 0 ? e.type == 'T' : pass == 1 ? e.type != 'P' : true;
      if (match && (!victim || e.order < victim->order)) victim = &e;
    }
  }
  if (!victim) victim = &g_q[0];
  g_dropped++;
  err::latch(pc::ERR_QUEUE, (uint16_t)g_dropped);
  return victim;
}

Event* beginEvent(char type) {
  Event* e = alloc();
  memset(e, 0, sizeof(*e));
  e->used = true;
  e->type = type;
  e->seq = ++g_seq;
  e->order = ++g_order;
  e->tms = clk::ms();
  e->atMs = millis();
  return e;
}

void escStr(const char* s, char* out, size_t n) {          // " and \ inside JSON strings
  size_t o = 0;
  for (; *s && o + 2 < n; s++) { if (*s == '"' || *s == '\\') out[o++] = '\\'; out[o++] = *s; }
  out[o] = '\0';
}
void fmtT10(char* b, size_t n, int32_t t10) { snprintf(b, n, "%s%ld.%ld", t10 < 0 ? "-" : "", (long)(labs(t10) / 10), (long)(labs(t10) % 10)); }
}  // namespace

void begin() {
  memset(g_q, 0, sizeof(g_q));
  g_order = 0;
  g_seq = 0;
  g_dropped = 0;
}

// oldest first, a few per pass; an event is removed once the broker has taken it
void poll() {
  if (!mqtt::configured()) {                                 // no broker configured: nothing to keep events for
    for (int i = 0; i < EVENT_QUEUE_SIZE; i++) g_q[i].used = false;
    return;
  }
  if (!mqtt::connected()) return;
  const uint32_t now = millis();
  if (now - g_lastTry < 20UL) return;
  g_lastTry = now;
  for (int sent = 0; sent < 4; sent++) {
    Event* next = nullptr;
    for (int i = 0; i < EVENT_QUEUE_SIZE; i++)
      if (g_q[i].used && (!next || g_q[i].order < next->order)) next = &g_q[i];
    if (!next) return;
    char json[320];
    const char* kind = "";
    if (!toJson(*next, kind, json, sizeof(json))) { next->used = false; continue; }
    char sub[24];
    snprintf(sub, sizeof(sub), "event/%s", kind);
    if (!mqtt::publish(sub, json, false)) return;           // broker went away: keep it, try later
    next->used = false;
    g_published++;
  }
}

void pushPress(uint32_t count, uint16_t durMs, int16_t tempC10, bool relayOn) {
  Event* e = beginEvent('P');
  e->a = count; e->b = durMs; e->c = tempC10; e->d = relayOn ? 1 : 0;
  // who pressed with which die mounted: "<operator>|<die>", published as two fields
  snprintf(e->text, sizeof(e->text), "%s|%s", session::operatorId(), session::dieId());
}
void pushNfc(char kind, bool relayAfter, const char* text) {
  Event* e = beginEvent('N');
  e->a = (uint8_t)kind; e->d = relayAfter ? 1 : 0;
  pc::sanitize(e->text, sizeof(e->text), text);
}
void pushRelay(bool on, uint8_t reason) {
  Event* e = beginEvent('R');
  e->a = on ? 1 : 0; e->b = reason;
  plog::log("machine relay %s (reason %u)", on ? "ON" : "OFF", reason);
}
void pushOutput(uint8_t index, bool on, uint8_t reason) {
  Event* e = beginEvent('O');
  e->a = index; e->b = on ? 1 : 0; e->d = reason;
}
void pushCall(uint8_t type, bool raised) {
  Event* e = beginEvent('C');
  e->a = type; e->b = raised ? 1 : 0;
}
void pushError(uint8_t code, bool raised, uint16_t detail) {
  Event* e = beginEvent('E');
  e->a = code; e->b = raised ? 1 : 0; e->d = detail;
  plog::log("E%02u %s %s", code, pc::errorText(code), raised ? "raised" : "cleared");
}
void pushTemp(int16_t tempC10) {
  Event* e = beginEvent('T');
  e->c = tempC10;
}

uint8_t pending() {
  uint8_t n = 0;
  for (int i = 0; i < EVENT_QUEUE_SIZE; i++) if (g_q[i].used) n++;
  return n;
}
uint32_t dropped() { return g_dropped; }
uint32_t published() { return g_published; }

// MQTT payloads (docs/PROTOCOL.md); ts = unix seconds (0 = no clock yet), ms = unix milliseconds
bool toJson(const Event& e, const char*& kind, char* json, size_t n) {
  char text[100], ttxt[16], pair[104];
  uint64_t tms = e.tms;
  if (!tms && clk::valid()) tms = clk::ms() - (uint64_t)(millis() - e.atMs);    // happened before NTP answered: back-date it now
  const unsigned long tsec = (unsigned long)(tms / 1000ULL);
  int w = 0;
  switch (e.type) {
    case 'P': {
      fmtT10(ttxt, sizeof(ttxt), e.c == INT16_MIN ? 0 : e.c);
      char op[48], die[48];
      pc::copyStr(pair, e.text, sizeof(pair));
      char* bar = strchr(pair, '|');
      if (bar) { *bar = '\0'; pc::copyStr(die, bar + 1, sizeof(die)); } else die[0] = '\0';
      pc::copyStr(op, pair, sizeof(op));
      escStr(op, pair, sizeof(pair));  pc::copyStr(op, pair, sizeof(op));
      escStr(die, text, sizeof(text)); pc::copyStr(die, text, sizeof(die));
      w = snprintf(json, n, "{\"count\":%lu,\"duration\":%lu,\"temperature\":%s,\"relay\":%u,\"operator\":\"%s\",\"die\":\"%s\",\"seq\":%u,\"ts\":%lu}",
                   (unsigned long)e.a, (unsigned long)e.b, ttxt, e.d, op, die, e.seq, tsec);
      kind = "press";
      break;
    }
    case 'N': {
      escStr(e.text, text, sizeof(text));
      escStr(session::dieId(), pair, sizeof(pair));        // which die was mounted when the card was read
      w = snprintf(json, n, "{\"tag\":\"%s\",\"kind\":\"%c\",\"relay\":%u,\"operator\":\"%s\",\"die\":\"%s\",\"seq\":%u,\"ts\":%lu}",
                   text, (char)e.a, e.d, session::operatorId(), pair, e.seq, tsec);
      kind = "nfc";
      break;
    }
    case 'R':
      w = snprintf(json, n, "{\"relay\":%lu,\"reason\":%lu,\"seq\":%u,\"ts\":%lu}", (unsigned long)e.a, (unsigned long)e.b, e.seq, tsec);
      kind = "relay";
      break;
    case 'O':
      w = snprintf(json, n, "{\"output\":%lu,\"relay\":%lu,\"state\":%lu,\"reason\":%u,\"seq\":%u,\"ts\":%lu}", (unsigned long)e.a, (unsigned long)e.a + 1,
                   (unsigned long)e.b, e.d, e.seq, tsec);
      kind = "output";
      break;
    case 'C':
      w = snprintf(json, n, "{\"type\":%lu,\"state\":%lu,\"seq\":%u,\"ts\":%lu}", (unsigned long)e.a, (unsigned long)e.b, e.seq, tsec);
      kind = "call";
      break;
    case 'E':
      w = snprintf(json, n, "{\"code\":%lu,\"state\":%lu,\"detail\":%u,\"text\":\"%s\",\"seq\":%u,\"ts\":%lu}", (unsigned long)e.a, (unsigned long)e.b, e.d,
                   pc::errorText((uint8_t)e.a), e.seq, tsec);
      kind = "error";
      break;
    case 'T':
      fmtT10(ttxt, sizeof(ttxt), e.c);
      w = snprintf(json, n, "{\"temperature\":%s,\"seq\":%u,\"ts\":%lu}", ttxt, e.seq, tsec);
      kind = "temperature";
      break;
    default:
      return false;
  }
  return w > 0 && (size_t)w < n;
}

}  // namespace events
