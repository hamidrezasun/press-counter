#ifndef INFLUXDB_H
#define INFLUXDB_H

#include <asyncHTTPrequest.h>
#include "ConfigManager.h"

class InfluxDB {
public:
  InfluxDB(ConfigManager& cfg);
  void begin();
  void sendData(const String& measurement, const String& fields);
  void sendDeviceInfo();
  bool getLastWriteFailed() const;

private:
  ConfigManager& configManager;
  asyncHTTPrequest http;
  bool lastWriteFailed;
};

#endif