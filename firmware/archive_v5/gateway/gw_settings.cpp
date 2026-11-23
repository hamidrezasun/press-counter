#include "gw_settings.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "gw_config.h"

GwSettings settings::cfg;

namespace {
void cp(char* dst, size_t n, const char* src) { pc::copyStr(dst, src ? src : "", n); }
}

namespace settings {

void defaults() {
  memset(&cfg, 0, sizeof(cfg));
  cp(cfg.device, sizeof(cfg.device), "press-1");
  cfg.mqttPort = 1883;
  cp(cfg.mqttTopic, sizeof(cfg.mqttTopic), "press");
  cp(cfg.mqttClient, sizeof(cfg.mqttClient), "press-gw");
  cfg.autoUpdate = 0;
  cfg.checkMinutes = 360;
  cp(cfg.ntpHost, sizeof(cfg.ntpHost), "pool.ntp.org");
  cp(cfg.otaPass, sizeof(cfg.otaPass), "hydropress");
}

bool begin() {
  defaults();
  if (!LittleFS.begin()) {
    LittleFS.format();
    if (!LittleFS.begin()) return false;
  }
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  DynamicJsonDocument d(2048);
  const DeserializationError e = deserializeJson(d, f);
  f.close();
  if (e) return false;
  JsonObject o = d.as<JsonObject>();
  auto s = [&](const char* k, char* dst, size_t n) { if (o.containsKey(k)) cp(dst, n, o[k] | ""); };
  s("wifiSsid", cfg.wifiSsid, sizeof(cfg.wifiSsid));
  s("wifiPass", cfg.wifiPass, sizeof(cfg.wifiPass));
  s("device", cfg.device, sizeof(cfg.device));
  s("mqttHost", cfg.mqttHost, sizeof(cfg.mqttHost));
  s("mqttTopic", cfg.mqttTopic, sizeof(cfg.mqttTopic));
  s("mqttClient", cfg.mqttClient, sizeof(cfg.mqttClient));
  s("mqttUser", cfg.mqttUser, sizeof(cfg.mqttUser));
  s("mqttPass", cfg.mqttPass, sizeof(cfg.mqttPass));
  s("updateUrl", cfg.updateUrl, sizeof(cfg.updateUrl));
  s("updateToken", cfg.updateToken, sizeof(cfg.updateToken));
  s("ntpHost", cfg.ntpHost, sizeof(cfg.ntpHost));
  s("adminPass", cfg.adminPass, sizeof(cfg.adminPass));
  s("otaPass", cfg.otaPass, sizeof(cfg.otaPass));
  cfg.mqttPort = o["mqttPort"] | cfg.mqttPort;
  cfg.autoUpdate = o["autoUpdate"] | cfg.autoUpdate;
  cfg.checkMinutes = o["checkMinutes"] | cfg.checkMinutes;
  if (cfg.checkMinutes < 5) cfg.checkMinutes = 5;
  return true;
}

bool save() {
  DynamicJsonDocument d(2048);
  d["wifiSsid"] = cfg.wifiSsid;
  d["wifiPass"] = cfg.wifiPass;
  d["device"] = cfg.device;
  d["mqttHost"] = cfg.mqttHost;
  d["mqttPort"] = cfg.mqttPort;
  d["mqttTopic"] = cfg.mqttTopic;
  d["mqttClient"] = cfg.mqttClient;
  d["mqttUser"] = cfg.mqttUser;
  d["mqttPass"] = cfg.mqttPass;
  d["updateUrl"] = cfg.updateUrl;
  d["updateToken"] = cfg.updateToken;
  d["autoUpdate"] = cfg.autoUpdate;
  d["checkMinutes"] = cfg.checkMinutes;
  d["ntpHost"] = cfg.ntpHost;
  d["adminPass"] = cfg.adminPass;
  d["otaPass"] = cfg.otaPass;
  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  const size_t n = serializeJson(d, f);
  f.close();
  return n > 0;
}

bool hasWifi() { return cfg.wifiSsid[0] != '\0'; }

}  // namespace settings
