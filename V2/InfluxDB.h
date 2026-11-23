// InfluxDB 2.x line-protocol writer with a small RAM queue.
//  - sendData() never blocks and never drops data just because a request is still in flight
//  - while WiFi / the server is down, up to QUEUE_SIZE points are kept and sent later
//  - points carry an NTP timestamp (ms) when the clock is valid, so late delivery keeps the true time
#ifndef INFLUXDB_H
#define INFLUXDB_H

#include <asyncHTTPrequest.h>
#include "ConfigManager.h"

class InfluxDB {
public:
  explicit InfluxDB(ConfigManager& cfg);
  void begin();                                   // starts NTP
  void sendData(const char* measurement, const char* fields);
  void sendDeviceInfo();
  void loop();                                    // call from loop(): delivers queued points
  bool getLastWriteFailed() const { return _lastFailed; }
  size_t pending() const { return _count; }

private:
  static const uint8_t QUEUE_SIZE = 10;
  static const size_t  LINE_LEN   = 320;

  ConfigManager& _config;
  asyncHTTPrequest _http;
  char   _queue[QUEUE_SIZE][LINE_LEN];
  uint8_t _head = 0, _count = 0;
  bool   _inFlight = false;
  bool   _lastFailed = false;
  unsigned long _retryAt = 0;

  void push(const char* line);
  void pop();
  bool configured() const;
  void trySend();
};

#endif
