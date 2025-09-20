#include "WebServerManager.h"
#include <FS.h>
#include <ESP8266WiFi.h>

WebServerManager::WebServerManager(ESP8266WebServer& srv, ConfigManager& cfg, ControlManager& ctrl, NfcManager& nfc, PressCounter& pc)
  : server(srv), configManager(cfg), controlManager(ctrl), nfcManager(nfc), pressCounter(pc), lastTagData("No tag read") {
}

void WebServerManager::setupRoutes() {
  server.on("/", HTTP_GET, std::bind(&WebServerManager::handleRoot, this));
  server.on(
    "/upload",
    HTTP_POST,
    [this]() {
      server.send(200, "text/plain", "Upload complete");
    },
    [this]() {
      handleUploadHtml();
    }
  );
  server.on("/save", HTTP_GET, std::bind(&WebServerManager::handleGetSettings, this));
  server.on("/save", HTTP_POST, std::bind(&WebServerManager::handleSaveSettings, this));
  server.on("/relay", HTTP_POST, std::bind(&WebServerManager::handleRelayToggle, this));
  server.on("/buzzer_error", HTTP_POST, std::bind(&WebServerManager::handleBuzzerError, this));
  server.on("/buzzer_warning", HTTP_POST, std::bind(&WebServerManager::handleBuzzerWarning, this));
  server.on("/buzzer_success", HTTP_POST, std::bind(&WebServerManager::handleBuzzerSuccess, this));
  server.on("/write_tag", HTTP_POST, std::bind(&WebServerManager::handleWriteTag, this));
  server.on("/status", HTTP_GET, std::bind(&WebServerManager::handleStatus, this));
  server.on("/status/file", HTTP_GET, std::bind(&WebServerManager::handleFileStatus, this));
  server.on("/press_counter", HTTP_GET, std::bind(&WebServerManager::handlePressCounterStatus, this));
}

void WebServerManager::handlePressCounterStatus() {
  String json = "{";
  json += "\"count\":" + String(pressCounter.getCount()) + ",";
  json += "\"lastPulseDuration\":" + String(pressCounter.getLastPulseDuration()) + ",";
  json += "\"lastPulseTime\":" + String(pressCounter.getLastPulseTime()) + ",";
  json += "\"inPulseTrain\":" + String(pressCounter.getInPulseTrain() ? "true" : "false") + ",";
  json += "\"pulseTrainStart\":" + String(pressCounter.getPulseTrainStart());
  json += "}";
  server.send(200, "application/json", json);
}

void WebServerManager::handleGetSettings() {
  Config config = configManager.get();
  String json = "{";
  json += "\"device\":\"" + String(config.device) + "\",";
  json += "\"influxHost\":\"" + String(config.influxHost) + "\",";
  json += "\"influxPort\":" + String(config.influxPort) + ",";
  json += "\"influxBucket\":\"" + String(config.influxBucket) + "\",";
  json += "\"influxOrg\":\"" + String(config.influxOrg) + "\",";
  json += "\"influxToken\":\"" + String(config.influxToken) + "\",";
  json += "\"minPulseDuration\":" + String(config.minPulseDuration);
  json += "}";
  server.send(200, "application/json", json);
}

void WebServerManager::handleRoot() {
  if (SPIFFS.exists("/index.html")) {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
      Serial.println("Failed to open index.html for reading");
      server.send(500, "text/plain", "Failed to open file");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  } else {
    Serial.println("index.html not found in SPIFFS");
    String html = "<!DOCTYPE html><html><head><title>Upload Interface</title>";
    html += "<style>body{font-family:Arial,sans-serif;padding:20px}.control{margin:10px 0}input,button{padding:8px;margin:5px}</style>";
    html += "<h2>No Interface Found</h2>";
    html += "<p>Please upload an index.html file to customize the interface.</p>";
    html += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
    html += "<div class='control'><input type='file' name='htmlFile' accept='.html'></div>";
    html += "<div class='control'><button type='submit'>Upload HTML</button></div>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  }
}

void WebServerManager::handleUploadHtml() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Uploading: %s, Size: %u\n", upload.filename.c_str(), upload.totalSize);
    if (upload.filename.isEmpty()) {
      server.send(400, "text/plain", "No file selected");
      return;
    }
    uploadFile = SPIFFS.open("/index.html", "w");
    if (!uploadFile) {
      Serial.println("Failed to open file for writing");
      server.send(500, "text/plain", "Failed to open file");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.println("File uploaded successfully");
    }
  }
}

void WebServerManager::handleSaveSettings() {
  bool updated = false;
  Config config = configManager.get();
  if (server.hasArg("device") && server.arg("device").length() > 0) {
    strlcpy(config.device, server.arg("device").c_str(), sizeof(config.device));
    updated = true;
  }
  if (server.hasArg("influxHost") && server.arg("influxHost").length() > 0) {
    strlcpy(config.influxHost, server.arg("influxHost").c_str(), sizeof(config.influxHost));
    updated = true;
  }
  if (server.hasArg("influxPort") && server.arg("influxPort").length() > 0) {
    config.influxPort = server.arg("influxPort").toInt();
    updated = true;
  }
  if (server.hasArg("influxBucket") && server.arg("influxBucket").length() > 0) {
    strlcpy(config.influxBucket, server.arg("influxBucket").c_str(), sizeof(config.influxBucket));
    updated = true;
  }
  if (server.hasArg("influxOrg") && server.arg("influxOrg").length() > 0) {
    strlcpy(config.influxOrg, server.arg("influxOrg").c_str(), sizeof(config.influxOrg));
    updated = true;
  }
  if (server.hasArg("influxToken") && server.arg("influxToken").length() > 0) {
    strlcpy(config.influxToken, server.arg("influxToken").c_str(), sizeof(config.influxToken));
    updated = true;
  }
  if (server.hasArg("minPulseDuration") && server.arg("minPulseDuration").length() > 0) {
    config.minPulseDuration = server.arg("minPulseDuration").toInt();
    updated = true;
  }
  if (updated) {
    configManager.set(config);
    if (configManager.save()) {
      Serial.println("Configuration saved successfully");
      pressCounter.updateMinPulseDuration();
      server.send(200, "text/plain", "Settings saved");
    } else {
      Serial.println("Failed to save configuration");
      server.send(500, "text/plain", "Failed to save settings");
    }
  } else {
    Serial.println("No valid settings provided to save");
    server.send(400, "text/plain", "No valid settings provided");
  }
}

void WebServerManager::handleRelayToggle() {
  controlManager.setRelayState(false, true);
  server.send(200, "text/plain", "Relay toggled");
}

void WebServerManager::handleBuzzerError() {
  controlManager.playErrorSound();
  server.send(200, "text/plain", "Error sound played");
}

void WebServerManager::handleBuzzerWarning() {
  controlManager.playWarningSound();
  server.send(200, "text/plain", "Warning sound played");
}

void WebServerManager::handleBuzzerSuccess() {
  controlManager.playSuccessSound();
  server.send(200, "text/plain", "Success sound played");
}

void WebServerManager::handleWriteTag() {
  if (server.hasArg("tagText") && server.arg("tagText").length() > 0) {
    nfcManager.setPendingWrite(server.arg("tagText").c_str());
    lastTagData = "Waiting for NFC tag...";
  } else {
    lastTagData = "No data provided to write";
    controlManager.playErrorSound();
  }
  server.send(200, "text/plain", lastTagData);
}

void WebServerManager::handleStatus() {
  DeviceStatus status = controlManager.getStatus();
  String json = "{";
  json += "\"relayStatus\":\"" + String(status.relayOn ? "On" : "Off") + "\",";
  json += "\"buzzerStatus\":\"" + String(status.buzzerOn ? "On" : "Off") + "\",";
  json += "\"buttonStatus\":\"" + String(status.buttonPressed ? "Pressed" : "Released") + "\",";
  json += "\"tagData\":\"" + String(nfcManager.getLastTagData()) + "\"";
  json += "}";
  lastTagData = nfcManager.getLastTagData();
  server.send(200, "application/json", json);
}

void WebServerManager::handleFileStatus() {
  String json = "{";
  json += "\"fileExists\":" + String(SPIFFS.exists("/index.html") ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void WebServerManager::begin() {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }
  server.onNotFound([this]() {
    server.send(404, "text/plain", "Not found");
  });
  setupRoutes();
  server.begin();
  Serial.println("Web server started");

  // Initialize ArduinoOTA
  ArduinoOTA.setHostname(configManager.get().device);
  ArduinoOTA.setPassword("admin");
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update: Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Update: Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Update: Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Update service started");
}

void WebServerManager::handleClient() {
  server.handleClient();
  ArduinoOTA.handle();
}

void WebServerManager::updateStatus() {
  lastTagData = nfcManager.getLastTagData();
}