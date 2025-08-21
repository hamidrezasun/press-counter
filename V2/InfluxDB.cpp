#include "InfluxDB.h"
#include <ESP8266WiFi.h>

InfluxDB::InfluxDB(ConfigManager& cfg) : configManager(cfg), lastWriteFailed(false) {}

void InfluxDB::begin() {}

void InfluxDB::sendData(const String& measurement, const String& fields) {
  Config cfg = configManager.get();
  if (cfg.influxHost.isEmpty() || cfg.influxBucket.isEmpty() || cfg.influxOrg.isEmpty() || cfg.influxToken.isEmpty()) {
    Serial.println("InfluxDB not configured");
    lastWriteFailed = true;
    return;
  }

  String url = "http://" + cfg.influxHost + ":" + String(cfg.influxPort) + "/api/v2/write?org=" + cfg.influxOrg + "&bucket=" + cfg.influxBucket + "&precision=s";

  String data = measurement + ",device=" + cfg.device + " " + fields;
  Serial.println("Sending to InfluxDB: " + data);

  http.onReadyStateChange([](void* arg, asyncHTTPrequest* request, int readyState) {
    InfluxDB* influx = static_cast<InfluxDB*>(arg);
    if (readyState == 4) {
      if (request->responseHTTPcode() >= 200 && request->responseHTTPcode() < 300) {
        Serial.println("InfluxDB data sent successfully");
        influx->lastWriteFailed = false;
      } else {
        Serial.println("InfluxDB send failed: HTTP " + String(request->responseHTTPcode()));
        Serial.println("Response: " + request->responseText());
        influx->lastWriteFailed = true;
      }
    }
  }, this);

  if (http.readyState() == 0 || http.readyState() == 4) {
    if (http.open("POST", url.c_str())) {
      http.setReqHeader("Content-Type", "text/plain");
      http.setReqHeader("Authorization", ("Token " + cfg.influxToken).c_str());
      http.setTimeout(5);
      http.send(data);
    } else {
      Serial.println("Failed to open HTTP request");
      lastWriteFailed = true;
    }
  } else {
    Serial.println("HTTP request not ready, state: " + String(http.readyState()));
    lastWriteFailed = true;
  }
}

void InfluxDB::sendDeviceInfo() {
  String fields = "ip=\"" + WiFi.localIP().toString() + "\"";
  fields += ",rssi=" + String(WiFi.RSSI());
  fields += ",mac=\"" + WiFi.macAddress() + "\"";
  
  sendData("device_info", fields);
}

bool InfluxDB::getLastWriteFailed() const {
  return lastWriteFailed;
}