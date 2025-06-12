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
#define AP_SSID WIFI_SSID // Same as upstream AP
#define AP_PASSWORD WIFI_PASSWORD // Same as upstream AP
#define AP_CHANNEL 6 // Must match upstream AP channel

// Timing Constants
#define EDGE_TIMEOUT 100 // Milliseconds
#define MIN_PULSE_TRAIN_DURATION 500 // Microseconds
#define SEND_RETRY_INTERVAL 5000 // Milliseconds
#define EEPROM_SIZE 6
#define INIT_MARKER 0xAA
#define EEPROM_SAVE_THRESHOLD 10
#define EEPROM_SAVE_INTERVAL 60000 // Milliseconds
#define SERVER_INTERVAL 100 // Milliseconds
#define QUEUE_SIZE 100

ESP8266WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point pulsePoint("production");
ESP8266WebServer server(80);

// Pulse Queue
struct PulseData {
  uint16_t count;
  unsigned long duration_ms;
};
PulseData pulseQueue[QUEUE_SIZE];
volatile uint16_t queueHead = 0;
volatile uint16_t queueTail = 0;

// State Variables
struct {
  uint16_t pulseCount;
  uint16_t failedPulseCount;
  unsigned long lastPulseDuration;
  unsigned long lastSendAttempt;
} state = {0, 0, 0, 0};

// Interrupt Variables
volatile unsigned long lastEdgeTime = 0;
volatile bool inPulseTrain = false;
volatile unsigned long pulseTrainStart = 0;
volatile bool edgeDetected = false;

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
    <div class="status">WiFi Signal (STA): <span class="%SIGNAL_CLASS%">%SIGNAL%</span></div>
    <div class="status">IP Address (STA): %IP%</div>
    <div class="status">Bridge SSID: %AP_SSID%</div>
    <div class="status">Connected Clients: %AP_CLIENTS%</div>
  </div>
  
  <div class="card">
    <h2>Count Statistics</h2>
    <div class="status">Current Count: %COUNT%</div>
    <div class="status">Failed Counts: %FAILED%</div>
    <div class="status">Last Duration: %DURATION% ms</div>
    <div class="status">Queue Usage: %QUEUE_USAGE%</div>
  </div>
  
  <div class="card">
    <h2>Firmware Update</h2>
    <form method='POST' action='/update' enctype='multipart/form-data'>
      <input type='file' name='update'>
      <стаbutton class='btn' type='submit'>Update Firmware</button>
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
  static uint16_t unsavedPulses = 0;
  static unsigned long lastEepromSave = 0;
  
  unsavedPulses++;
  
  if (unsavedPulses >= EEPROM_SAVE_THRESHOLD || (millis() - lastEepromSave > EEPROM_SAVE_INTERVAL)) {
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
    unsavedPulses = 0;
    lastEepromSave = millis();
  }
}

void handleRoot() {
  String page = htmlPage;
  int rssi = WiFi.RSSI();
  
  page.replace("%DEVICE%", DEVICE);
  page.replace("%SIGNAL%", getSignalQuality(rssi) + " (" + String(rssi) + " dBm)");
  page.replace("%SIGNAL_CLASS%", getSignalClass(rssi));
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%AP_SSID%", AP_SSID);
  page.replace("%AP_CLIENTS%", String(WiFi.softAPgetStationNum()));
  page.replace("%COUNT%", String(state.pulseCount));
  page.replace("%FAILED%", String(state.failedPulseCount));
  page.replace("%DURATION%", String(state.lastPulseDuration));
  page.replace("%QUEUE_USAGE%", String(queueHead - queueTail));
  
  server.send(200, "text/html", page);
}

void ICACHE_RAM_ATTR handlePulseInterrupt() {
  unsigned long currentTime = micros();
  lastEdgeTime = currentTime;
  
  if (!inPulseTrain) {
    inPulseTrain = true;
    pulseTrainStart = currentTime;
  }
  edgeDetected = true;
}

bool enqueuePulse(uint16_t count, unsigned long duration_ms) {
  uint16_t nextHead = (queueHead + 1) % QUEUE_SIZE;
  if (nextHead == queueTail) return false; // Queue full
  pulseQueue[queueHead] = {count, duration_ms};
  queueHead = nextHead;
  return true;
}

bool dequeuePulse(PulseData& pulse) {
  if (queueHead == queueTail) return false; // Queue empty
  pulse = pulseQueue[queueTail];
  queueTail = (queueTail + 1) % QUEUE_SIZE;
  return true;
}

void sendToInfluxDB(uint16_t count, unsigned long duration_ms, bool fromQueue = false) {
  pulsePoint.clearFields();
  pulsePoint.addField("count", count);
  pulsePoint.addField("duration_ms", duration_ms);

  if (client.writePoint(pulsePoint)) {
    if (!fromQueue) {
      state.pulseCount++;
      saveCountToEEPROM();
    }
  } else {
    if (!fromQueue) {
      if (enqueuePulse(count, duration_ms)) {
        state.failedPulseCount++;
        saveCountToEEPROM(true);
      }
    } else {
      enqueuePulse(count, duration_ms);
    }
  }
  state.lastSendAttempt = millis();
}

void processQueue() {
  PulseData pulse;
  while (dequeuePulse(pulse)) {
    sendToInfluxDB(pulse.count, pulse.duration_ms, true);
    yield();
  }
}

void sendDeviceInfo() {
  pulsePoint.clearFields();
  pulsePoint.addField("count", 0);
  pulsePoint.addField("duration_ms", 0);
  client.writePoint(pulsePoint);

  Point infoPoint("device_info");
  infoPoint.addTag("device", DEVICE);
  infoPoint.addField("ip", WiFi.localIP().toString());
  infoPoint.addField("rssi", WiFi.RSSI());
  infoPoint.addField("ap_clients", WiFi.softAPgetStationNum());
  client.writePoint(infoPoint);
}

void setup() {
  system_update_cpu_freq(160); // Overclock to 160 MHz
  pinMode(INPUT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), handlePulseInterrupt, CHANGE);
  loadCountFromEEPROM();

  // Configure WiFi in STA+AP mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  // Configure AP with same SSID and password, no local IP assignment
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, 4); // Max 4 clients
  // Note: No softAPConfig to avoid local DHCP; rely on upstream DHCP

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
  }

  // Initialize web server
  server.on("/", handleRoot);
  server.on("/queue", []() {
    server.send(200, "text/plain", "Queue Size: " + String(queueHead - queueTail));
  });
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

  sendDeviceInfo();
}

void loop() {
  static unsigned long lastServerHandle = 0;

  if (millis() - lastServerHandle > SERVER_INTERVAL) {
    server.handleClient();
    lastServerHandle = millis();
  }

  if (edgeDetected) {
    noInterrupts();
    unsigned long edgeTime = lastEdgeTime;
    unsigned long startTime = pulseTrainStart;
    bool pulseActive = inPulseTrain;
    interrupts();

    if (pulseActive && (micros() - edgeTime > EDGE_TIMEOUT * 1000)) {
      unsigned long duration = (edgeTime - startTime) / 1000;
      if (duration >= MIN_PULSE_TRAIN_DURATION) {
        sendToInfluxDB(1, duration);
      }
      noInterrupts();
      inPulseTrain = false;
      edgeDetected = false;
      interrupts();
    }
  }

  if (millis() - state.lastSendAttempt > SEND_RETRY_INTERVAL) {
    processQueue();
  }
}