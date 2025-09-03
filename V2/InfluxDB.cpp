#include "InfluxDB.h"
#include <ESP8266WiFi.h>

InfluxDB::InfluxDB(ConfigManager& cfg) : configManager(cfg), lastWriteFailed(false) {}

void InfluxDB::begin() {}

void InfluxDB::sendData(const char* measurement, const char* fields) {
  Config cfg = configManager.get();
  if (strlen(cfg.influxHost) == 0 || strlen(cfg.influxBucket) == 0 || 
      strlen(cfg.influxOrg) == 0 || strlen(cfg.influxToken) == 0) {
    Serial.println("InfluxDB not configured");
    lastWriteFailed = true;
    return;
  }

  char url[256];
  snprintf(url, sizeof(url), "http://%s:%d/api/v2/write?org=%s&bucket=%s&precision=s",
           cfg.influxHost, cfg.influxPort, cfg.influxOrg, cfg.influxBucket);

  char data[512];
  snprintf(data, sizeof(data), "%s,device=%s %s", measurement, cfg.device, fields);
  Serial.print("Sending to InfluxDB: ");
  Serial.println(data);

  http.onReadyStateChange([](void* arg, asyncHTTPrequest* request, int readyState) {
    InfluxDB* influx = static_cast<InfluxDB*>(arg);
    if (readyState == 4) {
      if (request->responseHTTPcode() >= 200 && request->responseHTTPcode() < 300) {
        Serial.println("InfluxDB data sent successfully");
        influx->lastWriteFailed = false;
      } else {
        Serial.print("InfluxDB send failed: HTTP ");
        Serial.println(request->responseHTTPcode());
        Serial.print("Response: ");
        Serial.println(request->responseText());
        influx->lastWriteFailed = true;
      }
    }
  }, this);

  if (http.readyState() == 0 || http.readyState() == 4) {
    if (http.open("POST", url)) {
      http.setReqHeader("Content-Type", "text/plain");
      char authHeader[128];
      snprintf(authHeader, sizeof(authHeader), "Token %s", cfg.influxToken);
      http.setReqHeader("Authorization", authHeader);
      http.setTimeout(5);
      http.send(data);
    } else {
      Serial.println("Failed to open HTTP request");
      lastWriteFailed = true;
    }
  } else {
    Serial.print("HTTP request not ready, state: ");
    Serial.println(http.readyState());
    lastWriteFailed = true;
  }
}

void InfluxDB::sendDeviceInfo() {
  char fields[256];
  snprintf(fields, sizeof(fields), "ip=\"%s\",rssi=%d,mac=\"%s\"",
           WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.macAddress().c_str());
  
  sendData("device_info", fields);
}

bool InfluxDB::getLastWriteFailed() const {
  return lastWriteFailed;
}