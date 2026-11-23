#include "InfluxDB.h"
#include <ESP8266WiFi.h>
#include <time.h>
#include <sys/time.h>

// Escape characters that are special in line-protocol tag values: , = and space
static void escapeTagValue(const char* src, char* dst, size_t dstSize) {
  size_t di = 0;
  for (size_t si = 0; src[si] && di + 2 < dstSize; si++) {
    const char c = src[si];
    if (c == ',' || c == ' ' || c == '=') dst[di++] = '\\';
    dst[di++] = c;
  }
  dst[di] = '\0';
}

InfluxDB::InfluxDB(ConfigManager& cfg) : _config(cfg) {}

void InfluxDB::begin() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");   // UTC; harmless when there is no internet
  _http.onReadyStateChange([](void* arg, asyncHTTPrequest* req, int state) {
    if (state != 4) return;
    InfluxDB* self = static_cast<InfluxDB*>(arg);
    const int code = req->responseHTTPcode();
    self->_inFlight = false;
    if (code >= 200 && code < 300) {
      self->_lastFailed = false;
      self->pop();                                       // delivered
    } else {
      self->_lastFailed = true;
      self->_retryAt = millis() + 5000UL;                // keep the point, retry in 5 s
      Serial.printf("[Influx] write failed, HTTP %d\n", code);
    }
  }, this);
}

bool InfluxDB::configured() const {
  const Config& c = _config.get();
  return c.influxHost[0] && c.influxBucket[0] && c.influxOrg[0] && c.influxToken[0];
}

void InfluxDB::push(const char* line) {
  if (_count == QUEUE_SIZE) {                            // full: drop the oldest (not the one in flight)
    if (_inFlight) return;
    _head = (_head + 1) % QUEUE_SIZE;
    _count--;
  }
  strlcpy(_queue[(_head + _count) % QUEUE_SIZE], line, LINE_LEN);
  _count++;
}

void InfluxDB::pop() {
  if (_count == 0) return;
  _head = (_head + 1) % QUEUE_SIZE;
  _count--;
}

void InfluxDB::sendData(const char* measurement, const char* fields) {
  if (!configured()) return;                             // not set up yet: nothing to log to
  const Config& cfg = _config.get();
  char dev[sizeof(cfg.device) * 2 + 1];
  escapeTagValue(cfg.device, dev, sizeof(dev));

  char line[LINE_LEN];
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec > 1700000000L) {                         // NTP is valid
    const unsigned long long ms = (unsigned long long)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
    snprintf(line, sizeof(line), "%s,device=%s %s %llu", measurement, dev, fields, ms);
  } else {                                               // no clock: let the server stamp it
    snprintf(line, sizeof(line), "%s,device=%s %s", measurement, dev, fields);
  }
  push(line);
  trySend();
}

void InfluxDB::trySend() {
  if (_inFlight || _count == 0 || WiFi.status() != WL_CONNECTED || millis() < _retryAt) return;
  const int rs = _http.readyState();
  if (rs != 0 && rs != 4) return;

  const Config& c = _config.get();
  char url[320];
  snprintf(url, sizeof(url), "http://%s:%d/api/v2/write?org=%s&bucket=%s&precision=ms",
           c.influxHost, c.influxPort, c.influxOrg, c.influxBucket);
  char auth[sizeof(c.influxToken) + 8];
  snprintf(auth, sizeof(auth), "Token %s", c.influxToken);

  if (!_http.open("POST", url)) { _retryAt = millis() + 5000UL; return; }
  _http.setReqHeader("Content-Type", "text/plain; charset=utf-8");
  _http.setReqHeader("Authorization", auth);
  _http.setTimeout(5);
  _inFlight = true;
  if (!_http.send(_queue[_head])) { _inFlight = false; _retryAt = millis() + 5000UL; }
}

void InfluxDB::loop() { trySend(); }

void InfluxDB::sendDeviceInfo() {
  char fields[160];
  snprintf(fields, sizeof(fields), "ip=\"%s\",rssi=%d,mac=\"%s\"",
           WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.macAddress().c_str());
  sendData("device_info", fields);
}
