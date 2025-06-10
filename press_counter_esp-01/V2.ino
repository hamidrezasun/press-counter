#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <InfluxDbClient.h>
#include <EEPROM.h>

// Configuration
#define DEVICE "test"
#define WIFI_SSID "TW factory"
#define WIFI_PASSWORD "e8cac589tw"
#define INFLUXDB_URL "http://192.168.1.37:8086"
#define INFLUXDB_TOKEN "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
#define INFLUXDB_ORG "tooswasher"
#define INFLUXDB_BUCKET "manufacture"
#define INPUT_PIN 2

// Timing Constants
#define EDGE_TIMEOUT 100
#define MIN_PULSE_TRAIN_DURATION 500
#define REPORT_INTERVAL 1
#define SEND_RETRY_INTERVAL 5000
#define EEPROM_SIZE 6
#define INIT_MARKER 0xAA

ESP8266WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point pulsePoint("production");
ESP8266WebServer server(80);

// State Variables
struct {
  uint16_t pulseCount;
  uint16_t failedPulseCount;
  unsigned long lastPulseDuration;
  unsigned long lastEdgeTime;
  unsigned long pulseTrainStart;
  unsigned long lastSendAttempt;
  bool inPulseTrain;
  bool shouldSend;
} state = {0, 0, 0, 0, 0, 0, false, false};

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>%DEVICE% Status</title>
  <style>
    body {font-family: Arial; margin: 20px;}
    h1 {color: #0066cc;}
    .card {
      background: #f9f9f9;
      border: 1px solid #ddd;
      padding: 20px;
      border-radius: 5px;
      margin-bottom: 20px;
    }
    .status {margin-bottom: 10px;}
    .btn {
      background: #4CAF50;
      color: white;
      padding: 10px 15px;
      border: none;
      border-radius: 4px;
      cursor: pointer;
    }
    .signal-excellent { color: green; }
    .signal-good { color: limegreen; }
    .signal-fair { color: orange; }
    .signal-weak { color: red; }
  </style>
</head>
<body>
  <h1>%DEVICE% Status</h1>
  
  <div class="card">
    <h2>Device Information</h2>
    <div class="status">WiFi Signal: <span class="%SIGNAL_CLASS%">%SIGNAL%</span></div>
    <div class="status">IP Address: %IP%</div>
  </div>
  
  <div class="card">
    <h2>Count Statistics</h2>
    <div class="status">Current Count: %COUNT%</div>
    <div class="status">Failed Counts: %FAILED%</div>
    <div class="status">Last Duration: %DURATION% ms</div>
  </div>
  
  <div class="card">
    <h2>Firmware Update</h2>
    <form method='POST' action='/update' enctype='multipart/form-data'>
      <input type='file' name='update'>
      <button class='btn' type='submit'>Update Firmware</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

String getSignalQuality(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  return "Weak";
}

String getSignalClass(int rssi) {
  if (rssi >= -50) return "signal-excellent";
  if (rssi >= -60) return "signal-good";
  if (rssi >= -70) return "signal-fair";
  return "signal-weak";
}

void loadCountFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) != INIT_MARKER) {
    EEPROM.write(0, INIT_MARKER);
    EEPROM.write(1, 0);
    EEPROM.write(2, 0);
    EEPROM.write(3, 0);
    EEPROM.write(4, 0);
    EEPROM.commit();
  } else {
    state.pulseCount = EEPROM.read(1) | (EEPROM.read(2) << 8);
    state.failedPulseCount = EEPROM.read(3) | (EEPROM.read(4) << 8);
  }
  EEPROM.end();
}

void saveCountToEEPROM(bool saveFailed = false) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, INIT_MARKER);
  EEPROM.write(1, state.pulseCount & 0xFF);
  EEPROM.write(2, (state.pulseCount >> 8) & 0xFF);
  if (saveFailed) {
    EEPROM.write(3, state.failedPulseCount & 0xFF);
    EEPROM.write(4, (state.failedPulseCount >> 8) & 0xFF);
  }
  EEPROM.commit();
  EEPROM.end();
}

void handleRoot() {
  String page = htmlPage;
  int rssi = WiFi.RSSI();
  
  page.replace("%DEVICE%", DEVICE);
  page.replace("%SIGNAL%", getSignalQuality(rssi) + " (" + String(rssi) + " dBm)");
  page.replace("%SIGNAL_CLASS%", getSignalClass(rssi));
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%COUNT%", String(state.pulseCount));
  page.replace("%FAILED%", String(state.failedPulseCount));
  page.replace("%DURATION%", String(state.lastPulseDuration));
  
  server.send(200, "text/html", page);
}

void handlePulseTrain() {
  static int lastState = LOW;
  int currentState = digitalRead(INPUT_PIN);

  if (currentState != lastState) {
    state.lastEdgeTime = millis();
    if (!state.inPulseTrain) {
      state.inPulseTrain = true;
      state.pulseTrainStart = state.lastEdgeTime;
    }
    lastState = currentState;
  }

  if (state.inPulseTrain && (millis() - state.lastEdgeTime > EDGE_TIMEOUT)) {
    unsigned long duration = state.lastEdgeTime - state.pulseTrainStart;

    if (duration >= MIN_PULSE_TRAIN_DURATION) {
      state.pulseCount++;
      state.lastPulseDuration = duration;
      saveCountToEEPROM();
      if (state.pulseCount % REPORT_INTERVAL == 0) {
        state.shouldSend = true;
      }
    }
    state.inPulseTrain = false;
  }
}

void sendToInfluxDB() {
  if (state.pulseCount == 0 && state.failedPulseCount == 0) return;

  // Retry failed pulses first
  if (state.failedPulseCount > 0) {
    pulsePoint.clearFields();
    pulsePoint.addField("count", state.failedPulseCount);
    pulsePoint.addField("duration_ms", state.lastPulseDuration);

    if (client.writePoint(pulsePoint)) {
      state.failedPulseCount = 0;
      saveCountToEEPROM(true);
    }
  }

  // Send current pulses
  if (state.pulseCount > 0) {
    pulsePoint.clearFields();
    pulsePoint.addField("count", state.pulseCount);
    pulsePoint.addField("duration_ms", state.lastPulseDuration);

    if (!client.writePoint(pulsePoint)) {
      state.failedPulseCount += state.pulseCount;
      state.pulseCount = 0;
      saveCountToEEPROM(true);
    } else {
      state.pulseCount = 0;
      saveCountToEEPROM();
    }
  }

  state.lastSendAttempt = millis();
}

void sendDeviceInfo() {
  // Send zero-count record
  pulsePoint.clearFields();
  pulsePoint.addField("count", 0);
  pulsePoint.addField("duration_ms", 0);
  client.writePoint(pulsePoint);

  // Send IP address record
  Point infoPoint("device_info");
  infoPoint.addTag("device", DEVICE);
  infoPoint.addField("ip", WiFi.localIP().toString());
  infoPoint.addField("rssi", WiFi.RSSI());
  client.writePoint(infoPoint);
}

void setup() {
  pinMode(INPUT_PIN, INPUT);
  loadCountFromEEPROM();

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
  }

  // Initialize web server
  server.on("/", handleRoot);
  server.on("/update", HTTP_GET, []() {
    server.send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><button>Update</button></form>");
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      WiFiUDP::stopAll();
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
    yield();
  });
  
  server.begin();

  pulsePoint.addTag("device", DEVICE);
  
  // Send initial device information
  sendDeviceInfo();
}

void loop() {
  server.handleClient();
  handlePulseTrain();

  if (state.shouldSend && !state.inPulseTrain) {
    if (millis() - state.lastSendAttempt > SEND_RETRY_INTERVAL) {
      sendToInfluxDB();
      state.shouldSend = false;
    }
  }
}