#include "events.h"
#include "store.h"
#include "clock.h"
#include "link.h"
#include "errors.h"

namespace events {
namespace {
Event    g_q[EVENT_QUEUE_SIZE];
uint32_t g_order = 0;
uint16_t g_seq = 0;
uint32_t g_dropped = 0;

Event* alloc() {
  for (int i = 0; i < EVENT_QUEUE_SIZE; i++)
    if (!g_q[i].used) return &g_q[i];
  // full: sacrifice the least valuable entry - oldest temperature report, else the oldest non-press event, else the oldest
  Event* victim = nullptr;
  for (int pass = 0; pass < 3 && !victim; pass++) {
    for (int i = 0; i < EVENT_QUEUE_SIZE; i++) {
      Event& e = g_q[i];
      if (e.inFlight) continue;
      const bool match = pass == 0 ? e.type == 'T' : pass == 1 ? e.type != 'P' : true;
      if (match && (!victim || e.order < victim->order)) victim = &e;
    }
  }
  if (!victim) victim = &g_q[0];
  g_dropped++;
  err::latch(pc::ERR_QUEUE, (uint16_t)g_dropped);
  return victim;
}

Event* begin_event(char type) {
  Event* e = alloc();
  memset(e, 0, sizeof(*e));
  e->used = true;
  e->type = type;
  e->seq = ++g_seq;
  e->order = ++g_order;
  clk::now(e->tsec, e->tms);
  return e;
}

// hand the oldest events to the ESP (a few per pass)
void deliver() {
  const uint32_t now = millis();
  const bool linkOk = link::gatewayReady();
  int sentThisPass = 0;

  uint32_t lastOrder = 0;
  while (sentThisPass < 3) {
    Event* next = nullptr;
    for (int i = 0; i < EVENT_QUEUE_SIZE; i++) {
      Event& e = g_q[i];
      if (!e.used || e.order <= lastOrder) continue;
      if (!next || e.order < next->order) next = &e;
    }
    if (!next) break;
    lastOrder = next->order;
    Event& e = *next;

    if (e.inFlight && now - e.sentAt < EVENT_RETRY_MS) continue;       // waiting for the ESP's acknowledgement
    if (!linkOk) { e.inFlight = false; break; }                         // nobody to give it to: keep it queued
    char payload[LINE_PAYLOAD];
    if (!format(e, payload, sizeof(payload))) { e.used = false; continue; }
    if (!link::send(payload)) break;                                    // TX buffer full: try again next loop
    e.inFlight = true;
    e.sentAt = now;
    e.tries++;
    sentThisPass++;
  }
}
}  // namespace

void begin() {
  memset(g_q, 0, sizeof(g_q));
  g_order = 0;
  g_seq = 0;
  g_dropped = 0;
}

void poll() { deliver(); }

void pushPress(uint32_t count, uint16_t durMs, int16_t tempC10, bool relayOn) {
  Event* e = begin_event('P');
  e->a = count; e->b = durMs; e->c = tempC10; e->d = relayOn ? 1 : 0;
}
void pushNfc(char kind, bool relayAfter, const char* text) {
  Event* e = begin_event('N');
  e->a = (uint8_t)kind; e->d = relayAfter ? 1 : 0;
  pc::sanitize(e->text, sizeof(e->text), text);
}
void pushRelay(bool on, uint8_t reason) {
  Event* e = begin_event('R');
  e->a = on ? 1 : 0; e->b = reason;
}
void pushOutput(uint8_t index, bool on, uint8_t reason) {
  Event* e = begin_event('O');
  e->a = index; e->b = on ? 1 : 0; e->d = reason;
}
void pushCall(uint8_t type, bool raised) {
  Event* e = begin_event('C');
  e->a = type; e->b = raised ? 1 : 0;
}
void pushError(uint8_t code, bool raised, uint16_t detail) {
  Event* e = begin_event('E');
  e->a = code; e->b = raised ? 1 : 0; e->d = detail;
}
void pushTemp(int16_t tempC10) {
  Event* e = begin_event('T');
  e->c = tempC10;
}

void onAck(uint16_t seq) {
  for (int i = 0; i < EVENT_QUEUE_SIZE; i++)
    if (g_q[i].used && g_q[i].seq == seq) { g_q[i].used = false; return; }
}

uint8_t pending() {
  uint8_t n = 0;
  for (int i = 0; i < EVENT_QUEUE_SIZE; i++) if (g_q[i].used) n++;
  return n;
}
uint32_t dropped() { return g_dropped; }

// UART payloads (docs/PROTOCOL.md):
//   P,seq,count,dur,tempC10,relay,t      N,seq,kind,relay,t,text      R,seq,relay,reason,t
//   O,seq,n,state,reason,t     C,seq,type,state,t                   E,seq,code,state,detail,t    T,seq,tempC10,t
bool format(const Event& e, char* out, size_t n) {
  char t[20];
  pc::formatTime(t, sizeof(t), e.tsec, e.tms);
  int w = 0;
  switch (e.type) {
    case 'P':
      w = snprintf(out, n, "P,%u,%lu,%lu,%ld,%u,%s", e.seq, (unsigned long)e.a, (unsigned long)e.b, (long)e.c, e.d, t);
      break;
    case 'N':
      w = snprintf(out, n, "N,%u,%c,%u,%s,%s", e.seq, (char)e.a, e.d, t, e.text);
      break;
    case 'R':
      w = snprintf(out, n, "R,%u,%lu,%lu,%s", e.seq, (unsigned long)e.a, (unsigned long)e.b, t);
      break;
    case 'O':
      w = snprintf(out, n, "O,%u,%lu,%lu,%u,%s", e.seq, (unsigned long)e.a, (unsigned long)e.b, e.d, t);
      break;
    case 'C':
      w = snprintf(out, n, "C,%u,%lu,%lu,%s", e.seq, (unsigned long)e.a, (unsigned long)e.b, t);
      break;
    case 'E':
      w = snprintf(out, n, "E,%u,%lu,%lu,%u,%s", e.seq, (unsigned long)e.a, (unsigned long)e.b, e.d, t);
      break;
    case 'T':
      w = snprintf(out, n, "T,%u,%ld,%s", e.seq, (long)e.c, t);
      break;
    default:
      return false;
  }
  return w > 0 && (size_t)w < n;
}

}  // namespace events
