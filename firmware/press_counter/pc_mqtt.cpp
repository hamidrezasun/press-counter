#include "pc_mqtt.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "pc_errors.h"
#include "pc_events.h"
#include "pc_log.h"
#include "pc_net.h"
#include "pc_nfc.h"
#include "pc_outputs.h"
#include "pc_press.h"
#include "pc_sense.h"
#include "pc_session.h"
#include "pc_store.h"
#include "pc_temp.h"

namespace {
WiFiClient   g_netClient;
PubSubClient g_mqtt(g_netClient);
uint32_t g_try = 0, g_okAt = 0;
int16_t  g_lastMask = -1;                       // relay states last published (bit n-1 = relay n), -1 = publish all again

bool enabled() { return store::net.mqttHost[0] != '\0'; }

void escStr(const char* s, char* out, size_t n) {
  size_t o = 0;
  for (; *s && o + 2 < n; s++) { if (*s == '"' || *s == '\\') out[o++] = '\\'; out[o++] = *s; }
  out[o] = '\0';
}

int parseState(const char* v) {
  if (!v) return -1;
  if (!strcasecmp(v, "on") || !strcmp(v, "1") || !strcasecmp(v, "true")) return 1;
  if (!strcasecmp(v, "off") || !strcmp(v, "0") || !strcasecmp(v, "false")) return 0;
  if (!strcasecmp(v, "toggle") || !strcmp(v, "2")) return 2;
  return -1;
}

uint8_t relayMask() { return (uint8_t)((session::relayOn() ? 1 : 0) | (outputs::mask() << 1)); }

// relay n: 1 = K1 (the machine relay), 2..5 = OUT1..OUT4.  state 0 off, 1 on, 2 toggle
void relayCommand(int n, int state) {
  if (state < 0) return;
  if (n >= 2 && n <= 5) { outputs::command((uint8_t)(n - 1), (uint8_t)state, pc::RR_REMOTE); return; }
  if (n != 1) return;
  if (state == 0) session::remoteCommand(pc::CMD_RELAY_OFF, 0, pc::RR_REMOTE);
  else if (state == 1) session::remoteCommand(pc::CMD_RELAY_ON, 0, pc::RR_REMOTE);
  else session::remoteCommand(pc::CMD_RELAY_TOGGLE, 0, pc::RR_REMOTE);
}

void fmtMilli(char* b, size_t n, uint16_t mv) { snprintf(b, n, "%u.%02u", mv / 1000, (mv % 1000) / 10); }

void callback(char* topic, uint8_t* payload, unsigned int len) {
  char base[96];
  snprintf(base, sizeof(base), "%s/%s/", store::net.mqttTopic, store::cfg.name);
  if (strncmp(topic, base, strlen(base)) != 0) return;
  const char* rest = topic + strlen(base);
  int rn = 0;
  if (sscanf(rest, "relay/%d/set", &rn) == 1) {                // <prefix>/<device>/relay/<n>/set   ON | OFF | TOGGLE
    char v[12];
    const size_t m = len < sizeof(v) - 1 ? len : sizeof(v) - 1;
    memcpy(v, payload, m);
    v[m] = '\0';
    relayCommand(rn, parseState(v));
    return;
  }
  if (strcmp(rest, "cmd") != 0) return;
  DynamicJsonDocument d(384);
  if (deserializeJson(d, payload, len)) return;
  const char* cmd = d["cmd"] | "";
  const int arg = d["arg"] | 0;
  plog::log("MQTT command %s", cmd);
  if (!strcmp(cmd, "relay")) {                                  // {"cmd":"relay","relay":N,"state":"on|off|toggle"}  (relay 1 if omitted)
    int st = -1;
    if (d["state"].is<const char*>()) st = parseState(d["state"].as<const char*>());
    else if (d["state"].is<bool>()) st = d["state"].as<bool>() ? 1 : 0;
    else if (d["state"].is<int>()) st = d["state"].as<int>();
    relayCommand(d["relay"] | 1, st);
  } else if (!strcmp(cmd, "relay_off")) relayCommand(1, 0);
  else if (!strcmp(cmd, "relay_on")) relayCommand(1, 1);               // refused unless 'remote start' is enabled
  else if (!strcmp(cmd, "out_on")) relayCommand(arg + 1, 1);           // arg = output 1..4
  else if (!strcmp(cmd, "out_off")) relayCommand(arg + 1, 0);
  else if (!strcmp(cmd, "reset")) session::remoteCommand(pc::CMD_RESET_CNT, 0, pc::RR_REMOTE);
  else if (!strcmp(cmd, "ack")) session::remoteCommand(pc::CMD_ACK_CALL, (uint8_t)arg, pc::RR_REMOTE);
  else if (!strcmp(cmd, "buzz")) session::remoteCommand(pc::CMD_BUZZ, (uint8_t)arg, pc::RR_REMOTE);
  else if (!strcmp(cmd, "reboot")) session::remoteCommand(pc::CMD_REBOOT, 0, pc::RR_REMOTE);
  else if (!strcmp(cmd, "clear_errors")) session::remoteCommand(pc::CMD_CLEAR_ERR, 0, pc::RR_REMOTE);
  else if (!strcmp(cmd, "call")) session::remoteCommand(pc::CMD_CALL, (uint8_t)arg, pc::RR_REMOTE);
  else if (!strcmp(cmd, "write_tag")) { const char* t = d["text"] | ""; if (t[0]) nfc::queueWrite(t); }
}

void service() {
  if (!enabled() || !net::wifiUp()) return;
  if (g_mqtt.connected()) { g_mqtt.loop(); g_okAt = millis(); return; }
  if (millis() - g_try < MQTT_RETRY_MS) return;
  g_try = millis();
  g_mqtt.setServer(store::net.mqttHost, store::net.mqttPort);
  g_mqtt.setBufferSize(768);
  g_mqtt.setSocketTimeout(4);                   // the loop (press counting is interrupt driven) must not hang for 15 s
  g_mqtt.setKeepAlive(30);
  g_mqtt.setCallback(callback);
  char will[128];
  snprintf(will, sizeof(will), "%s/%s/online", store::net.mqttTopic, store::cfg.name);
  const char* user = store::net.mqttUser[0] ? store::net.mqttUser : nullptr;
  const char* pass = store::net.mqttUser[0] ? store::net.mqttPass : nullptr;
  const char* client = store::net.mqttClient[0] ? store::net.mqttClient : store::cfg.name;
  if (g_mqtt.connect(client, user, pass, will, 1, true, "offline")) {
    g_mqtt.publish(will, "online", true);
    char sub[128];
    snprintf(sub, sizeof(sub), "%s/%s/cmd", store::net.mqttTopic, store::cfg.name);
    g_mqtt.subscribe(sub);
    snprintf(sub, sizeof(sub), "%s/%s/relay/+/set", store::net.mqttTopic, store::cfg.name);
    g_mqtt.subscribe(sub);
    g_lastMask = -1;
    g_okAt = millis();
    plog::log("MQTT connected to %s:%u", store::net.mqttHost, store::net.mqttPort);
    mqtt::publishInfo();
  } else {
    plog::log("MQTT connect to %s failed (state %d)", store::net.mqttHost, g_mqtt.state());
  }
}
}  // namespace

namespace mqtt {

void begin() { g_okAt = millis(); }

void reconfigure() {
  if (g_mqtt.connected()) g_mqtt.disconnect();
  g_try = 0;
}

void loop() {
  service();
  // E06: broker configured but not reachable for a long time
  err::set(pc::ERR_SERVER, enabled() && millis() - g_okAt > SERVER_ERROR_MS);
  const uint8_t mask = relayMask();
  if (g_mqtt.connected() && (int16_t)mask != g_lastMask) {
    bool ok = true;
    for (int n = 1; n <= 5; n++) {
      const uint8_t bit = (uint8_t)(1u << (n - 1));
      if (g_lastMask >= 0 && ((mask ^ (uint8_t)g_lastMask) & bit) == 0) continue;
      char sub[24];
      snprintf(sub, sizeof(sub), "relay/%d/state", n);
      ok = publish(sub, (mask & bit) ? "ON" : "OFF", true) && ok;
    }
    if (ok) g_lastMask = mask;
  }
}

bool configured() { return enabled(); }
bool connected() { return g_mqtt.connected(); }

bool publish(const char* sub, const char* payload, bool retain) {
  if (!g_mqtt.connected()) return false;
  char topic[160];
  snprintf(topic, sizeof(topic), "%s/%s/%s", store::net.mqttTopic, store::cfg.name, sub);
  return g_mqtt.publish(topic, payload, retain);
}

void publishInfo() {
  char json[256], name[48];
  escStr(store::cfg.name, name, sizeof(name));
  snprintf(json, sizeof(json), "{\"name\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"mac\":\"%s\",\"fw\":\"%s\",\"hw\":\"v6.2\"}", name, net::ip(), net::rssi(),
           WiFi.macAddress().c_str(), PC_FW_VERSION_STR);
  publish("info", json, true);
}

void publishStatus() {
  if (!g_mqtt.connected()) return;
  char json[640], op[48], die[48], t[16], v12[12], aux[12];
  const int16_t t10 = temp::tempC10();
  if (t10 == INT16_MIN) snprintf(t, sizeof(t), "null");
  else snprintf(t, sizeof(t), "%s%d.%d", t10 < 0 ? "-" : "", abs(t10) / 10, abs(t10) % 10);
  escStr(session::operatorId(), op, sizeof(op));
  escStr(session::dieId(), die, sizeof(die));
  fmtMilli(v12, sizeof(v12), sense::v12mV());
  fmtMilli(aux, sizeof(aux), sense::auxmV());
  snprintf(json, sizeof(json),
           "{\"relay\":%u,\"count\":%lu,\"lifetime\":%lu,\"lastDuration\":%lu,\"pressing\":%u,\"temperature\":%s,\"error\":%u,\"errors\":%u,"
           "\"operator\":\"%s\",\"die\":\"%s\",\"calls\":%u,\"session\":%lu,\"queued\":%u,\"uptime\":%lu,\"rssi\":%d,\"fw\":\"%s\","
           "\"relays\":[%u,%u,%u,%u,%u],\"outputs\":%u,\"v12\":%s,\"aux\":%s}",
           session::relayOn() ? 1 : 0, (unsigned long)press::count(), (unsigned long)store::lifetime(), (unsigned long)press::lastDurationMs(),
           press::active() ? 1 : 0, t, err::top(), err::mask(), op, die, session::callMask(), (unsigned long)session::sessionSeconds(),
           events::pending(), (unsigned long)(millis() / 1000UL), net::rssi(), PC_FW_VERSION_STR,
           session::relayOn() ? 1 : 0, outputs::state(1), outputs::state(2), outputs::state(3), outputs::state(4), outputs::mask(), v12, aux);
  publish("status", json, true);
  snprintf(json, sizeof(json), "{\"v12\":%s,\"aux\":%s}", v12, aux);
  publish("sensor/voltage", json, true);
}

}  // namespace mqtt
