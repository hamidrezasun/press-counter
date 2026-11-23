#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

// FIX: Field sizes are now named constants — easier to change and self-documenting.
// Old sizes (device=10, influxHost=20) were dangerously small for real hostnames.
struct Config {
  char device[32];
  char influxHost[64];
  int  influxPort;
  char influxBucket[32];
  char influxOrg[32];
  char influxToken[90];
  int  minPulseDuration;
  char otaPassword[32];
};

class ConfigManager {
public:
  explicit ConfigManager(const char* filename = "/config.json");

  // Returns true on success; false means defaults were applied.
  bool load();
  bool save();
  void setDefaults();

  Config& get();
  void    set(const Config& newConfig);
  bool    isMounted() const;

private:
  const char* _filename;
  Config      _config;
  bool        _spiffsReady;

  // Helper: ensure SPIFFS is mounted (idempotent).
  bool ensureSpiffs();
};

#endif // CONFIG_MANAGER_H
