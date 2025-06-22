#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <asyncHTTPrequest.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>

// --- Configuration ---
#define DEVICE "05015005"
#define WIFI_SSID "TW factory"
#define WIFI_PASSWORD "e8cac589tw"
#define INFLUXDB_URL "http://192.168.1.37:8086"
#define INFLUXDB_TOKEN "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA=="
#define INFLUXDB_ORG "tooswasher"
#define INFLUXDB_BUCKET "manufacture"
#define HTTP_REQUEST_TIMEOUT 0.3 //second
// --- Hardware & Time ---
#define INPUT_PIN 2

// --- Timing Constants ---
#define EDGE_TIMEOUT 30
#define MIN_PULSE_TRAIN_DURATION 250
#define REPORT_INTERVAL 1
#define SEND_RETRY_INTERVAL 5000

// --- EEPROM ---
#define EEPROM_SIZE 16
#define INIT_MARKER 0xAA

// --- Global Objects ---
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
asyncHTTPrequest request;

// --- State Variables ---
struct {
  uint32_t pulseCount;
  uint32_t failedPulseCount;
  uint32_t maxSendDuration_ms;
  unsigned long lastPulseDuration;
  unsigned long lastSendDuration_ms;
  unsigned long lastShortPulseDuration;
  unsigned long lastShortPulseTime;
  unsigned long lastEdgeTime;
  unsigned long pulseTrainStart;
  unsigned long lastSendAttempt;
  bool inPulseTrain;
  bool shouldSend;
  bool deviceInfoSent;
  bool requestInProgress;
  bool isDeviceInfoRequest;
  unsigned long requestStartTime;
} state = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, false, 0};

// --- Web Page Template ---
const char htmlPage[] PROGMEM = R"rawliteral(
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

String getSignalQuality(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  return "Weak";
}

void saveCountsToEEPROM() {
  static uint32_t lastSavedPulseCount = 0;
  static uint32_t lastSavedFailedCount = 0;
  static uint32_t lastSavedMaxDuration = 0;
  
  if (state.pulseCount != lastSavedPulseCount || 
      state.failedPulseCount != lastSavedFailedCount ||
      state.maxSendDuration_ms != lastSavedMaxDuration) {
    
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(0, INIT_MARKER);
    EEPROM.put(1, state.pulseCount);
    EEPROM.put(5, state.failedPulseCount);
    EEPROM.put(9, state.maxSendDuration_ms);
    EEPROM.commit();
    EEPROM.end();
    
    lastSavedPulseCount = state.pulseCount;
    lastSavedFailedCount = state.failedPulseCount;
    lastSavedMaxDuration = state.maxSendDuration_ms;
  }
}

void loadCountsFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) != INIT_MARKER) {
    EEPROM.write(0, INIT_MARKER);
    EEPROM.put(1, 0);
    EEPROM.put(5, 0);
    EEPROM.put(9, 0);
    EEPROM.commit();
    state.pulseCount = 0;
    state.failedPulseCount = 0;
    state.maxSendDuration_ms = 0;
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

void handleRoot() {
  String page = FPSTR(htmlPage);
  int rssi = WiFi.RSSI();
  
  String signalClass = "signal-weak";
  if (rssi >= -50) signalClass = "signal-excellent";
  else if (rssi >= -60) signalClass = "signal-good";
  else if (rssi >= -70) signalClass = "signal-fair";

  String shortPulseTime = "Never";
  if (state.lastShortPulseTime > 0) {
    unsigned long seconds = (millis() - state.lastShortPulseTime) / 1000;
    shortPulseTime = String(seconds) + " seconds ago";
  }

  page.replace("%DEVICE%", DEVICE);
  page.replace("%SIGNAL%", getSignalQuality(rssi) + " (" + String(rssi) + " dBm)");
  page.replace("%SIGNAL_CLASS%", signalClass);
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%COUNT%", String(state.pulseCount));
  page.replace("%FAILED%", String(state.failedPulseCount));
  page.replace("%DURATION%", String(state.lastPulseDuration));
  page.replace("%SEND_DURATION%", String(state.lastSendDuration_ms));
  page.replace("%MAX_SEND_DURATION%", String(state.maxSendDuration_ms));
  page.replace("%SHORT_PULSE_DURATION%", String(state.lastShortPulseDuration));
  page.replace("%SHORT_PULSE_TIME%", shortPulseTime);
  page.replace("%MIN_PULSE_DURATION%", String(MIN_PULSE_TRAIN_DURATION));

  server.send(200, "text/html", page);
}

void handleReset() {
  resetEEPROM();
  server.sendHeader("Location", "/");
  server.send(303);
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
    state.inPulseTrain = false;

    if (duration >= MIN_PULSE_TRAIN_DURATION) {
      state.pulseCount++;
      state.lastPulseDuration = duration;
      if (state.pulseCount % REPORT_INTERVAL == 0) {
        state.shouldSend = true;
      }
    } else {
      state.lastShortPulseDuration = duration;
      state.lastShortPulseTime = millis();
    }
  }
}

void onRequestResponse(void* opt, asyncHTTPrequest* req, int readyState) {
  if (readyState != 4) return;

  unsigned long sendDuration = millis() - state.requestStartTime;

  if (req->responseHTTPcode() == 204) {
    if (state.isDeviceInfoRequest) {
      state.deviceInfoSent = true;
    } else {
      state.pulseCount = 0;
      state.failedPulseCount = 0;
      saveCountsToEEPROM();
    }
    state.lastSendDuration_ms = sendDuration;
    if (sendDuration > state.maxSendDuration_ms) {
      state.maxSendDuration_ms = sendDuration;
      saveCountsToEEPROM();
    }
    state.shouldSend = false;
  } else {
    state.lastSendAttempt = millis();
    if (!state.isDeviceInfoRequest) {
      state.failedPulseCount += state.pulseCount;
      state.pulseCount = 0;
      saveCountsToEEPROM();
    }
    state.shouldSend = true;
  }

  state.requestInProgress = false;
  state.isDeviceInfoRequest = false;
}

bool sendToInfluxDB() {
  if (wifiMulti.run() != WL_CONNECTED || state.requestInProgress) {
    return false;
  }

  uint32_t totalToSend = state.pulseCount + state.failedPulseCount;
  if (totalToSend == 0) {
    state.shouldSend = false;
    return true;
  }

  String lineProtocol;
  lineProtocol.reserve(128);
  lineProtocol = "production,device=" + String(DEVICE) + " count=" + String(totalToSend) + "i";
  lineProtocol += ",pulse_duration_ms=" + String(state.lastPulseDuration) + "i";
  if (state.lastSendDuration_ms > 0) {
    lineProtocol += ",send_duration_ms=" + String(state.lastSendDuration_ms) + "i";
  }

  String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + 
               "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=ms";

  if (request.open("POST", url.c_str())) {
    request.setReqHeader("Authorization", ("Token " + String(INFLUXDB_TOKEN)).c_str());
    request.setReqHeader("Content-Type", "text/plain; charset=utf-8");
    state.requestStartTime = millis();
    state.isDeviceInfoRequest = false;
    state.requestInProgress = true;
    state.lastSendAttempt = millis();
    return request.send(lineProtocol.c_str());
  }
  
  state.lastSendAttempt = millis();
  state.shouldSend = true;
  return false;
}

void sendDeviceInfo() {
  if (state.deviceInfoSent || wifiMulti.run() != WL_CONNECTED || state.requestInProgress) return;

  String lineProtocol;
  lineProtocol.reserve(128);
  lineProtocol = "device_info,device=" + String(DEVICE) + 
                 " ip=\"" + WiFi.localIP().toString() + "\"" +
                 ",rssi=" + String(WiFi.RSSI()) + "i";

  String url = String(INFLUXDB_URL) + "/api/v2/write?org=" + String(INFLUXDB_ORG) + 
               "&bucket=" + String(INFLUXDB_BUCKET) + "&precision=ms";

  if (request.open("POST", url.c_str())) {
    request.setReqHeader("Authorization", ("Token " + String(INFLUXDB_TOKEN)).c_str());
    request.setReqHeader("Content-Type", "text/plain; charset=utf-8");
    state.requestStartTime = millis();
    state.isDeviceInfoRequest = true;
    state.requestInProgress = true;
    request.send(lineProtocol.c_str());
  }
}

void setup() {
  ESP.wdtEnable(8000);
  pinMode(INPUT_PIN, INPUT);
  
  loadCountsFromEEPROM();

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
  }
  request.setTimeout(HTTP_REQUEST_TIMEOUT);
  request.onReadyStateChange(onRequestResponse);
  server.on("/", handleRoot);
  server.on("/reset", HTTP_POST, handleReset);
  httpUpdater.setup(&server, "/update", "admin", "password");
  server.begin();
  
  sendDeviceInfo();
  if (state.pulseCount > 0 || state.failedPulseCount > 0) {
    state.shouldSend = true;
  }
}

void loop() {
  server.handleClient();
  handlePulseTrain();

  if (state.shouldSend && !state.inPulseTrain && !state.requestInProgress) {
    if (millis() - state.lastSendAttempt >= SEND_RETRY_INTERVAL) {
      sendToInfluxDB();
    }
  }
}