#include "WebServer.h"
#include <ArduinoJson.h>

void WebServer::init(ConfigManager* config, MQTTHandler* mqtt, DisplayHandler* display, HardwareHandler* hardware) {
  configMgr = config;
  mqttHdlr = mqtt;
  displayHdlr = display;
  hardwareHdlr = hardware;

  // Serve index.html from LittleFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(200, "text/html", "<h1>No index.html found</h1><p>Upload index.html to /api/upload</p>");
    }
  });

  // API endpoints
  server.on("/api", HTTP_GET, [this](AsyncWebServerRequest *request){
    handleApiGet(request);
  });

  server.on("/api", HTTP_POST, [this](AsyncWebServerRequest *request){
    handleApiPost(request);
  });

  server.on("/api/message", HTTP_POST, [this](AsyncWebServerRequest *request){
    handleMessage(request);
  });

  server.on("/api/vibrate", HTTP_POST, [this](AsyncWebServerRequest *request){
    handleVibrate(request);
  });

  server.on("/api/beep", HTTP_POST, [this](AsyncWebServerRequest *request){
    handleBeep(request);
  });

  // File upload
  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    // Response handled in onUpload
  }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    handleFileUpload(request, filename, index, data, len, final);
  });

  // Connect to WiFi
  Config cfg = configMgr->getConfig();
  if (cfg.ssid.length() > 0) {
    WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(500));
      Serial.print(".");
    }
    Serial.println();
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("No WiFi config, starting AP");
    WiFi.softAP("TWatch-Config", "12345678");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  server.begin();
  Serial.println("Web server started");
}

void WebServer::handle() {
  // Async server handles in background
}

void WebServer::handleApiGet(AsyncWebServerRequest *request) {
  Config cfg = configMgr->getConfig();
  DynamicJsonDocument doc(1024);
  doc["ssid"] = cfg.ssid;
  doc["password"] = cfg.password;
  doc["mqttServer"] = cfg.mqttServer;
  doc["mqttPort"] = cfg.mqttPort;
  doc["mqttUser"] = cfg.mqttUser;
  doc["mqttPass"] = cfg.mqttPass;
  doc["mqttTopic"] = cfg.mqttTopic;
  doc["displayTimeout"] = cfg.displayTimeout;

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebServer::handleApiPost(AsyncWebServerRequest *request) {
  Config cfg = configMgr->getConfig();
  if (request->hasParam("ssid", true)) cfg.ssid = request->getParam("ssid", true)->value();
  if (request->hasParam("password", true)) cfg.password = request->getParam("password", true)->value();
  if (request->hasParam("mqttServer", true)) cfg.mqttServer = request->getParam("mqttServer", true)->value();
  if (request->hasParam("mqttPort", true)) cfg.mqttPort = request->getParam("mqttPort", true)->value().toInt();
  if (request->hasParam("mqttUser", true)) cfg.mqttUser = request->getParam("mqttUser", true)->value();
  if (request->hasParam("mqttPass", true)) cfg.mqttPass = request->getParam("mqttPass", true)->value();
  if (request->hasParam("mqttTopic", true)) cfg.mqttTopic = request->getParam("mqttTopic", true)->value();
  if (request->hasParam("displayTimeout", true)) cfg.displayTimeout = request->getParam("displayTimeout", true)->value().toInt();
  configMgr->save(cfg);
  request->send(200, "application/json", "{\"status\":\"Config saved, restarting\"}");
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP.restart();
}

void WebServer::handleMessage(AsyncWebServerRequest *request) {
  if (request->hasParam("msg", true)) {
    String msg = request->getParam("msg", true)->value();
    displayHdlr->showMessage(msg.c_str());
    request->send(200, "application/json", "{\"status\":\"Message sent\"}");
  } else {
    request->send(400, "application/json", "{\"error\":\"Missing msg parameter\"}");
  }
}

void WebServer::handleVibrate(AsyncWebServerRequest *request) {
  hardwareHdlr->toggleVibrate();
  request->send(200, "application/json", "{\"status\":\"Vibrate toggled\"}");
}

void WebServer::handleBeep(AsyncWebServerRequest *request) {
  hardwareHdlr->playBeep();
  request->send(200, "application/json", "{\"status\":\"Beep played\"}");
}

void WebServer::handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (filename != "index.html") {
    request->send(400, "application/json", "{\"error\":\"Only index.html is accepted\"}");
    return;
  }

  static File file;
  if (index == 0) {
    file = LittleFS.open("/index.html", "w");
    if (!file) {
      request->send(500, "application/json", "{\"error\":\"Failed to open file for writing\"}");
      return;
    }
  }

  if (len > 0) {
    file.write(data, len);
  }

  if (final) {
    file.close();
    request->send(200, "application/json", "{\"status\":\"File uploaded\"}");
  }
}