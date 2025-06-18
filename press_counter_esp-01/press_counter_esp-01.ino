#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <InfluxDbClient.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <time.h>

// --- Configuration ---
#define DEVICE "05019902"
#define WIFI_SSID "TW factory"
#define WIFI_PASSWORD "e8cac589tw"
#define INFLUXDB_URL "http://192.168.1.37:8086"
#define INFLUXDB_TOKEN "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
#define INFLUXDB_ORG "tooswasher"
#define INFLUXDB_BUCKET "manufacture"

// --- Hardware & Time ---
#define INPUT_PIN 2
#define TIME_ZONE "IRST-3:30" // UTC+03:30 Iran Standard Time
#define NTP_SERVER "win-3d0te4stpjh.tooswasher.com"

// --- Timing Constants ---
#define EDGE_TIMEOUT 30
#define MIN_PULSE_TRAIN_DURATION 250
#define REPORT_INTERVAL 1
#define SEND_RETRY_INTERVAL 5000

// --- EEPROM ---
// EEPROM Layout:
// Byte 0:     INIT_MARKER (1 byte)
// Bytes 1-4:  pulseCount (4 bytes)
// Bytes 5-8:  failedPulseCount (4 bytes)
// Bytes 9-12: maxSendDuration_ms (4 bytes)
#define EEPROM_SIZE 16
#define INIT_MARKER 0xAA

// --- Global Objects ---
ESP8266WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point pulsePoint("production");
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// --- State Variables ---
struct {
  uint32_t pulseCount;
  uint32_t failedPulseCount;
  uint32_t maxSendDuration_ms;
  unsigned long lastPulseDuration;
  unsigned long lastSendDuration_ms;
  unsigned long lastShortPulseDuration;
  char lastShortPulseTimestamp[30];
  unsigned long lastEdgeTime;
  unsigned long pulseTrainStart;
  unsigned long lastSendAttempt;
  bool inPulseTrain;
  bool shouldSend;
} state = {0, 0, 0, 0, 0, 0, "N/A", 0, 0, 0, false, false};

// --- Web Page Template ---
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>%DEVICE% Status</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; }
    h1 { color: #0056b3; }
    .card { background: #fff; border: 1px solid #ddd; padding: 20px; border-radius: 8px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .status { margin-bottom: 10px; font-size: 1.1em; }
    .info { color: #5a5a5a; }
    .btn { background: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 1em; }
    .btn-danger { background-color: #f44336; }
    .btn:hover { opacity: 0.9; }
    .signal-excellent { color: #28a745; font-weight: bold; }
    .signal-good { color: #8fbf2f; font-weight: bold; }
    .signal-fair { color: #fd7e14; font-weight: bold; }
    .signal-weak { color: #dc3545; font-weight: bold; }
    input[type=file] { margin-bottom: 10px; }
  </style>
</head>
<body>
  <h1>%DEVICE% Status</h1>
  <div class="card">
    <h2>Device Information</h2>
    <div class="status">Time: %TIME%</div>
    <div class="status">WiFi Signal: <span class="%SIGNAL_CLASS%">%SIGNAL%</span></div>
    <div class="status">IP Address: %IP%</div>
  </div>
  <div class="card">
    <h2>Count Statistics & Reset</h2>
    <div class="status">Current Unsent Count: %COUNT%</div>
    <div class="status">Failed to Send Count: %FAILED%</div>
    <div class="status">Last Valid Pulse Duration: %DURATION% ms</div>
    <div class="status">Last Send Time: %SEND_DURATION% ms</div>
    <div class="status">Max Send Time: %MAX_SEND_DURATION% ms</div>
    <form method='POST' action='/reset' onsubmit="return confirm('WARNING: This will clear ALL counts and reset the maximum send time from EEPROM. This action cannot be undone. Are you sure?');" style="margin-top: 15px;">
      <button class='btn btn-danger' type='submit'>Reset Device & Clear EEPROM</button>
    </form>
  </div>
  <div class="card">
    <h2>Short Pulse Information</h2>
    <div class="status info">Last Short Pulse Duration: %SHORT_PULSE_DURATION% ms</div>
    <div class="status info">Time of Last Short Pulse: %SHORT_PULSE_TIME%</div>
    <p style="font-size:0.8em; color:#888;">This shows the last detected pulse that was too short to be counted as a valid production event (duration < %MIN_PULSE_DURATION%ms).</p>
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

// --- Helper Functions ---
String getSignalQuality(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  return "Weak";
}

// --- EEPROM Functions ---
void saveCountsToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, INIT_MARKER);
  EEPROM.put(1, state.pulseCount);
  EEPROM.put(5, state.failedPulseCount);
  EEPROM.put(9, state.maxSendDuration_ms);
  EEPROM.commit();
  EEPROM.end();
}

void loadCountsFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) != INIT_MARKER) {
    state.pulseCount = 0;
    state.failedPulseCount = 0;
    state.maxSendDuration_ms = 0;
    saveCountsToEEPROM();
  } else {
    EEPROM.get(1, state.pulseCount);
    EEPROM.get(5, state.failedPulseCount);
    EEPROM.get(9, state.maxSendDuration_ms);
  }
  EEPROM.end();
}

void resetEEPROM() {
  state.pulseCount = 0;
  state.failedPulseCount = 0;
  state.maxSendDuration_ms = 0;
  saveCountsToEEPROM();
}

// --- Web Server Handlers ---
void handleRoot() {
  String page = htmlPage;
  int rssi = WiFi.RSSI();
  
  time_t now = time(nullptr);
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
  
  page.replace("%DEVICE%", DEVICE);
  page.replace("%TIME%", timeStr);
  page.replace("%SIGNAL%", getSignalQuality(rssi) + " (" + String(rssi) + " dBm)");
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%COUNT%", String(state.pulseCount));
  page.replace("%FAILED%", String(state.failedPulseCount));
  page.replace("%DURATION%", String(state.lastPulseDuration));
  page.replace("%SEND_DURATION%", String(state.lastSendDuration_ms));
  page.replace("%MAX_SEND_DURATION%", String(state.maxSendDuration_ms));
  page.replace("%SHORT_PULSE_DURATION%", String(state.lastShortPulseDuration));
  page.replace("%SHORT_PULSE_TIME%", state.lastShortPulseTimestamp);
  page.replace("%MIN_PULSE_DURATION%", String(MIN_PULSE_TRAIN_DURATION));

  server.send(200, "text/html", page);
}

void handleReset() {
  resetEEPROM();
  server.sendHeader("Location", "/");
  server.send(303); // Redirect back to the root page
}

// --- Core Logic ---
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
    state.inPulseTrain = false;

    if (duration >= MIN_PULSE_TRAIN_DURATION) {
      state.pulseCount++;
      state.lastPulseDuration = duration;
      if (state.pulseCount % REPORT_INTERVAL == 0) {
        state.shouldSend = true;
      }
    } else {
      state.lastShortPulseDuration = duration;
      time_t now = time(nullptr);
      strftime(state.lastShortPulseTimestamp, sizeof(state.lastShortPulseTimestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    }
  }
}

void sendToInfluxDB() {
  if (wifiMulti.run() != WL_CONNECTED) {
    state.shouldSend = true;
    return;
  }

  uint32_t totalToSend = state.pulseCount + state.failedPulseCount;
  if (totalToSend == 0) return;

  pulsePoint.clearFields();
  pulsePoint.addField("count", totalToSend);
  pulsePoint.addField("pulse_duration_ms", state.lastPulseDuration);
  
  if (state.lastSendDuration_ms > 0) {
    pulsePoint.addField("send_duration_ms", state.lastSendDuration_ms);
  }

  unsigned long sendStartTime = millis();
  bool success = client.writePoint(pulsePoint);
  state.lastSendDuration_ms = millis() - sendStartTime;

  if (state.lastSendDuration_ms > state.maxSendDuration_ms) {
    state.maxSendDuration_ms = state.lastSendDuration_ms;
  }

  if (success) {
    state.pulseCount = 0;
    state.failedPulseCount = 0;
  } else {
    state.failedPulseCount = totalToSend;
    state.pulseCount = 0;
  }
  
  saveCountsToEEPROM();
  state.lastSendAttempt = millis();
}

void sendDeviceInfo() {
  if (wifiMulti.run() != WL_CONNECTED) return;

  Point infoPoint("device_info");
  infoPoint.addTag("device", DEVICE);
  infoPoint.addField("ip", WiFi.localIP().toString());
  infoPoint.addField("rssi", WiFi.RSSI());
  
  client.writePoint(infoPoint);
}

// --- Main Setup and Loop ---
void setup() {
  pinMode(INPUT_PIN, INPUT);
  
  loadCountsFromEEPROM();

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
  }

  configTzTime(TIME_ZONE, NTP_SERVER);
  
  server.on("/", handleRoot);
  server.on("/reset", HTTP_POST, handleReset);
  httpUpdater.setup(&server);
  server.begin();

  pulsePoint.addTag("device", DEVICE);
  
  sendDeviceInfo();
  if(state.pulseCount > 0 || state.failedPulseCount > 0){
    state.shouldSend = true;
  }
}

void loop() {
  server.handleClient();
  handlePulseTrain();

  if (state.shouldSend && !state.inPulseTrain) {
    if (state.failedPulseCount > 0 && (millis() - state.lastSendAttempt < SEND_RETRY_INTERVAL)) {
      // Wait for retry interval
    } else {
      sendToInfluxDB();
      state.shouldSend = false;
    }
  }
}