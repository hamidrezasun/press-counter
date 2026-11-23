#include "ConfigManager.h"

ConfigManager::ConfigManager(const char* filename)
  : _filename(filename), _spiffsReady(false) {}

// ── Private helper ────────────────────────────────────────────────────────────
bool ConfigManager::ensureSpiffs() {
  if (_spiffsReady) return true;
  // FIX (original bug): SPIFFS.begin() was called every load/save.
  // Calling it repeatedly after a successful mount is harmless on most
  // builds, but can cause problems on others. Mount once, remember the result.
  if (!SPIFFS.begin()) {
    Serial.println("[Config] Failed to mount SPIFFS");
    return false;
  }
  _spiffsReady = true;
  return true;
}

// ── Public API ────────────────────────────────────────────────────────────────
bool ConfigManager::load() {
  if (!ensureSpiffs()) {
    setDefaults();
    return false;
  }

  if (!SPIFFS.exists(_filename)) {
    Serial.println("[Config] No config file found, using defaults");
    setDefaults();
    return false;
  }

  File file = SPIFFS.open(_filename, "r");
  if (!file) {
    Serial.println("[Config] Failed to open config file");
    setDefaults();
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("[Config] JSON parse error: ");
    Serial.println(error.c_str());
    setDefaults();
    return false;
  }

  // Use strlcpy with the actual field sizes from the struct.
  strlcpy(_config.device,        doc["device"]        | "device", sizeof(_config.device));
  strlcpy(_config.influxHost,    doc["influxHost"]    | "",        sizeof(_config.influxHost));
  _config.influxPort =           doc["influxPort"]    | 8086;
  strlcpy(_config.influxBucket,  doc["influxBucket"]  | "",        sizeof(_config.influxBucket));
  strlcpy(_config.influxOrg,     doc["influxOrg"]     | "",        sizeof(_config.influxOrg));
  strlcpy(_config.influxToken,   doc["influxToken"]   | "",        sizeof(_config.influxToken));
  _config.minPulseDuration =     doc["minPulseDuration"] | 10;
  strlcpy(_config.otaPassword,   doc["otaPassword"]      | "hydropress", sizeof(_config.otaPassword));

  Serial.println("[Config] Loaded successfully");
  return true;
}

bool ConfigManager::save() {
  if (!ensureSpiffs()) return false;

  StaticJsonDocument<1024> doc;
  doc["device"]           = _config.device;
  doc["influxHost"]       = _config.influxHost;
  doc["influxPort"]       = _config.influxPort;
  doc["influxBucket"]     = _config.influxBucket;
  doc["influxOrg"]        = _config.influxOrg;
  doc["influxToken"]      = _config.influxToken;
  doc["minPulseDuration"] = _config.minPulseDuration;
  doc["otaPassword"]      = _config.otaPassword;

  File file = SPIFFS.open(_filename, "w");
  if (!file) {
    Serial.println("[Config] Failed to open config file for writing");
    return false;
  }

  // FIX: check serializeJson return value — it returns 0 on failure.
  size_t written = serializeJson(doc, file);
  file.close();

  if (written == 0) {
    Serial.println("[Config] serializeJson wrote 0 bytes — save failed");
    return false;
  }

  Serial.println("[Config] Saved successfully");
  return true;
}

void ConfigManager::setDefaults() {
  strlcpy(_config.device,       "device", sizeof(_config.device));
  strlcpy(_config.influxHost,   "",       sizeof(_config.influxHost));
  _config.influxPort = 8086;
  strlcpy(_config.influxBucket, "",       sizeof(_config.influxBucket));
  strlcpy(_config.influxOrg,    "",       sizeof(_config.influxOrg));
  strlcpy(_config.influxToken,  "",           sizeof(_config.influxToken));
  _config.minPulseDuration = 10;
  strlcpy(_config.otaPassword, "hydropress", sizeof(_config.otaPassword));
}

Config& ConfigManager::get()                    { return _config; }
void    ConfigManager::set(const Config& cfg)   { _config = cfg; }
bool    ConfigManager::isMounted() const        { return _spiffsReady; }
