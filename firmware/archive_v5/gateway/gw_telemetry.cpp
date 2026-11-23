#include "gw_telemetry.h"
#include "gw_state.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <sys/time.h>
#include "gw_settings.h"

// MQTT is the only backend.  Topics (prefix = setting "MQTT topic prefix", device = machine name):
//   <prefix>/<device>/online                 retained "online" / last will "offline"
//   <prefix>/<device>/status                 retained JSON, every 5 s (counter, relays, voltages, ...)
//   <prefix>/<device>/sensor/voltage         retained JSON {"v12":12.1,"v230":231.4}, every 5 s
//   <prefix>/<device>/info                   retained JSON (ip, rssi, mac, firmware versions), every 10 min
//   <prefix>/<device>/event/<press|nfc|relay|output|call|error|temperature>   JSON, one message per event
//   <prefix>/<device>/relay/<1..5>/state     retained "ON" / "OFF" (1 = K1 machine relay, 2..5 = OUT1..OUT4 = K2..K5)
//   <prefix>/<device>/relay/<1..5>/set       command: ON / OFF / TOGGLE (or 1 / 0); ON needs "remote start" in the counter's settings
//   <prefix>/<device>/cmd                    JSON command {"cmd":"relay_on|relay_off|out_on|out_off|relay|reset|ack|buzz|reboot|clear_errors|call","arg":N}
//                                            or {"cmd":"relay","relay":N,"state":"on|off|toggle"}
namespace {
bool     g_mqttOk = false;
WiFiClient   g_mqttNet;
PubSubClient g_mqtt(g_mqttNet);
uint32_t g_mqttTry = 0;
char     g_topicBase[96];
uint8_t  g_lastMask = 0xFF;                 // relay state last published (bit n-1 = relay n), 0xFF = unknown -> publish all
uint32_t g_maskAt = 0;

bool mqttEnabled() { return settings::cfg.mqttHost[0] != '\0'; }

void escStr(const char* s, char* out, size_t n) {          // " and \ inside JSON strings
  size_t o = 0;
  for (; *s && o + 2 < n; s++) { if (*s == '"' || *s == '\\') out[o++] = '\\'; out[o++] = *s; }
  out[o] = '\0';
}
void fmtT10(char* b, size_t n, int32_t t10) { snprintf(b, n, "%ld.%ld", (long)(t10 / 10), (long)(t10 < 0 ? -(t10 % 10) : t10 % 10)); }

uint64_t eventMs(const GwEvent& e) {
  if (e.tsec > 1700000000UL) return (uint64_t)e.tsec * 1000ULL + e.tms;
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec > 1700000000L) return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
  return 0;                                                    // no clock: the subscriber stamps it
}

const char* topicBase() {
  snprintf(g_topicBase, sizeof(g_topicBase), "%s", settings::cfg.mqttTopic);
  return g_topicBase;
}

bool mqttPublish(const char* sub, const char* payload, bool retain = false) {
  if (!g_mqtt.connected()) return false;
  char topic[160];
  snprintf(topic, sizeof(topic), "%s/%s/%s", topicBase(), gws::deviceName(), sub);
  return g_mqtt.publish(topic, payload, retain);
}

// relay n (1..5): 1 = K1 (the machine relay), 2..5 = OUT1..OUT4.  state: 0 off, 1 on, 2 toggle
void sendRelay(int n, int state) {
  char b[40];
  if (n == 1) {
    if (state == 2) snprintf(b, sizeof(b), "K,relay");
    else snprintf(b, sizeof(b), "K,relay,%d", state);
  } else if (n >= 2 && n <= 5) snprintf(b, sizeof(b), "K,out,%d:%d", n - 1, state);
  else return;
  gwlink::send(b);
}

int parseState(const char* v) {
  if (!v) return -1;
  if (!strcasecmp(v, "on") || !strcmp(v, "1") || !strcasecmp(v, "true")) return 1;
  if (!strcasecmp(v, "off") || !strcmp(v, "0") || !strcasecmp(v, "false")) return 0;
  if (!strcasecmp(v, "toggle") || !strcmp(v, "2")) return 2;
  return -1;
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int len) {
  const char* t = topic + strlen(topicBase());
  if (*t == '/') t++;
  char device[32];
  size_t i = 0;
  while (t[i] && t[i] != '/' && i + 1 < sizeof(device)) { device[i] = t[i]; i++; }
  device[i] = '\0';
  if (strcmp(device, gws::deviceName()) != 0) return;            // wildcard subscription: only commands for this machine
  const char* rest = t + i;
  if (*rest == '/') rest++;
  char b[40];

  int n = 0;
  if (sscanf(rest, "relay/%d/set", &n) == 1) {                    // <prefix>/<device>/relay/<n>/set   ON | OFF | TOGGLE
    char v[12];
    const size_t m = len < sizeof(v) - 1 ? len : sizeof(v) - 1;
    memcpy(v, payload, m);
    v[m] = '\0';
    const int st = parseState(v);
    if (st >= 0) sendRelay(n, st);
    return;
  }
  if (strcmp(rest, "cmd") != 0) return;
  DynamicJsonDocument d(256);
  if (deserializeJson(d, payload, len)) return;
  const char* cmd = d["cmd"] | "";
  const int arg = d["arg"] | 0;
  if (!strcmp(cmd, "relay")) {                                    // {"cmd":"relay","relay":N,"state":"on"}
    const int rn = d["relay"] | 0;
    int st = -1;
    if (d["state"].is<const char*>()) st = parseState(d["state"].as<const char*>());
    else if (d["state"].is<bool>()) st = d["state"].as<bool>() ? 1 : 0;
    else if (d["state"].is<int>()) st = d["state"].as<int>();
    if (st >= 0) sendRelay(rn, st);
    return;
  }
  if (!strcmp(cmd, "relay_off")) snprintf(b, sizeof(b), "K,relay,0");
  else if (!strcmp(cmd, "relay_on")) snprintf(b, sizeof(b), "K,relay,1");              // the counter refuses it unless 'remote start' is enabled
  else if (!strcmp(cmd, "out_on")) snprintf(b, sizeof(b), "K,out,%d:1", arg);          // arg = output 1..4
  else if (!strcmp(cmd, "out_off")) snprintf(b, sizeof(b), "K,out,%d:0", arg);
  else if (!strcmp(cmd, "reset")) snprintf(b, sizeof(b), "K,reset");
  else if (!strcmp(cmd, "ack")) snprintf(b, sizeof(b), "K,ack,all");
  else if (!strcmp(cmd, "buzz")) snprintf(b, sizeof(b), "K,buzz,%s", arg == 0 ? "success" : arg == 1 ? "warning" : "error");
  else if (!strcmp(cmd, "reboot")) snprintf(b, sizeof(b), "K,reboot");
  else if (!strcmp(cmd, "clear_errors")) snprintf(b, sizeof(b), "K,clr");
  else if (!strcmp(cmd, "call")) snprintf(b, sizeof(b), "K,call,%d", arg);
  else return;
  gwlink::send(b);
}

void mqttService() {
  if (!mqttEnabled() || WiFi.status() != WL_CONNECTED) { g_mqttOk = false; return; }
  if (g_mqtt.connected()) { g_mqtt.loop(); g_mqttOk = true; return; }
  g_mqttOk = false;
  if (millis() - g_mqttTry < MQTT_RETRY_MS) return;
  g_mqttTry = millis();
  g_mqtt.setServer(settings::cfg.mqttHost, settings::cfg.mqttPort);
  g_mqtt.setBufferSize(640);
  g_mqtt.setCallback(mqttCallback);
  char will[160];
  snprintf(will, sizeof(will), "%s/%s/online", topicBase(), gws::deviceName());
  const char* user = settings::cfg.mqttUser[0] ? settings::cfg.mqttUser : nullptr;
  const char* pass = settings::cfg.mqttUser[0] ? settings::cfg.mqttPass : nullptr;
  if (g_mqtt.connect(settings::cfg.mqttClient, user, pass, will, 1, true, "offline")) {
    g_mqtt.publish(will, "online", true);
    char sub[160];
    snprintf(sub, sizeof(sub), "%s/+/cmd", topicBase());
    g_mqtt.subscribe(sub);
    snprintf(sub, sizeof(sub), "%s/+/relay/+/set", topicBase());
    g_mqtt.subscribe(sub);
    g_lastMask = 0xFF;                                          // publish every relay state again
    g_mqttOk = true;
  }
}

void publishRelayStates(bool force) {
  const StmStatus& s = gwlink::status();
  if (!s.valid || !g_mqtt.connected()) return;
  const uint8_t mask = (uint8_t)((s.relay ? 1 : 0) | ((s.outs & 0x0F) << 1));
  if (!force && mask == g_lastMask) return;
  for (int n = 1; n <= 5; n++) {
    const uint8_t bit = (uint8_t)(1u << (n - 1));
    if (g_lastMask != 0xFF && !force && ((mask ^ g_lastMask) & bit) == 0) continue;
    char sub[24];
    snprintf(sub, sizeof(sub), "relay/%d/state", n);
    mqttPublish(sub, (mask & bit) ? "ON" : "OFF", true);
  }
  g_lastMask = mask;
}
}  // namespace

namespace tele {

void begin() {}

void reconfigure() {
  if (g_mqtt.connected()) g_mqtt.disconnect();
  g_mqttTry = 0;
}

void loop() {
  mqttService();
  if (millis() - g_maskAt >= 500UL) { g_maskAt = millis(); publishRelayStates(false); }
}

bool mqttConfigured() { return mqttEnabled(); }
bool mqttOk() { return g_mqttOk; }
bool serverOk() { return mqttEnabled() && g_mqttOk; }

void publishDeviceInfo() {
  const StmIdentity& id = gwlink::identity();
  char json[220], name[48];
  escStr(gws::deviceName(), name, sizeof(name));
  snprintf(json, sizeof(json), "{\"name\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"mac\":\"%s\",\"gateway_fw\":\"%s\",\"fw\":\"%s\"}", name,
           WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.macAddress().c_str(), GW_VERSION, id.ver);
  mqttPublish("info", json, true);
}

bool onEvent(const GwEvent& e) {
  if (e.type == 'N') pc::copyStr(gws::lastTag, e.text, sizeof(gws::lastTag));   // shown on the web page
  if (!mqttEnabled()) return true;                             // no broker configured: do not block the STM32 queue
  if (!g_mqtt.connected()) return false;                       // broker away: the event waits in the STM32 queue
  char text[100], ttxt[16], json[320];
  const uint64_t ms = eventMs(e);
  const unsigned long tsec = (unsigned long)(ms / 1000ULL);
  json[0] = '\0';
  const char* kind = "";
  switch (e.type) {
    case 'P':
      fmtT10(ttxt, sizeof(ttxt), e.c);
      snprintf(json, sizeof(json), "{\"count\":%ld,\"duration\":%ld,\"temperature\":%s,\"relay\":%u,\"ts\":%lu}", (long)e.a, (long)e.b, ttxt, e.d, tsec);
      kind = "press";
      break;
    case 'N':
      escStr(e.text, text, sizeof(text));
      snprintf(json, sizeof(json), "{\"tag\":\"%s\",\"kind\":\"%c\",\"relay\":%u,\"ts\":%lu}", text, (char)e.a, e.d, tsec);
      kind = "nfc";
      break;
    case 'R':
      snprintf(json, sizeof(json), "{\"relay\":%ld,\"reason\":%ld,\"ts\":%lu}", (long)e.a, (long)e.b, tsec);
      kind = "relay";
      break;
    case 'O':
      snprintf(json, sizeof(json), "{\"output\":%ld,\"relay\":%ld,\"state\":%ld,\"reason\":%u,\"ts\":%lu}", (long)e.a, (long)e.a + 1, (long)e.b, e.d, tsec);
      kind = "output";
      break;
    case 'C':
      snprintf(json, sizeof(json), "{\"type\":%ld,\"state\":%ld,\"ts\":%lu}", (long)e.a, (long)e.b, tsec);
      kind = "call";
      break;
    case 'E':
      snprintf(json, sizeof(json), "{\"code\":%ld,\"state\":%ld,\"detail\":%u,\"text\":\"%s\",\"ts\":%lu}", (long)e.a, (long)e.b, e.d, pc::errorText((uint8_t)e.a), tsec);
      kind = "error";
      break;
    case 'T':
      fmtT10(ttxt, sizeof(ttxt), e.c);
      snprintf(json, sizeof(json), "{\"temperature\":%s,\"ts\":%lu}", ttxt, tsec);
      kind = "temperature";
      break;
    default: return true;
  }
  char sub[32];
  snprintf(sub, sizeof(sub), "event/%s", kind);
  return mqttPublish(sub, json);
}

void publishStatus() {
  if (!mqttEnabled() || !g_mqtt.connected()) return;
  const StmStatus& s = gwlink::status();
  if (!s.valid) return;
  char json[460], t[16], op[48], die[48];
  fmtT10(t, sizeof(t), s.tempC10 == 9999 ? 0 : s.tempC10);
  escStr(s.op, op, sizeof(op));
  escStr(s.die, die, sizeof(die));
  snprintf(json, sizeof(json),
           "{\"relay\":%u,\"relays\":[%u,%u,%u,%u,%u],\"count\":%lu,\"lastDuration\":%lu,\"temperature\":%s,\"error\":%u,\"operator\":\"%s\",\"die\":\"%s\","
           "\"calls\":%u,\"outputs\":%u,\"v12\":%u.%02u,\"v230\":%u.%u,\"uptime\":%lu,\"rssi\":%d,\"fw\":\"%s\",\"gateway_fw\":\"%s\"}",
           s.relay, s.relay ? 1 : 0, s.outs & 1, (s.outs >> 1) & 1, (s.outs >> 2) & 1, (s.outs >> 3) & 1,
           (unsigned long)s.count, (unsigned long)s.lastDur, t, s.topErr, op, die, s.calls, s.outs,
           s.v12mV / 1000, (s.v12mV % 1000) / 10, s.vac10 / 10, s.vac10 % 10, (unsigned long)(millis() / 1000UL), WiFi.RSSI(),
           gwlink::identity().ver, GW_VERSION);
  mqttPublish("status", json, true);
  snprintf(json, sizeof(json), "{\"v12\":%u.%02u,\"v230\":%u.%u}", s.v12mV / 1000, (s.v12mV % 1000) / 10, s.vac10 / 10, s.vac10 % 10);
  mqttPublish("sensor/voltage", json, true);
  publishRelayStates(true);
}

}  // namespace tele
