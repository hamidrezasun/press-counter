#include "WebServerManager.h"
#include <ESP8266httpUpdate.h>
#include <FS.h>

static CallbackData deviceData, influxHostData, influxPortData, influxBucketData,
                   influxOrgData, influxTokenData, ntpData,
                   debounceDelayData, firmwareUrlData;

WebServerManager::WebServerManager(AsyncWebServer& srv, ConfigManager& cfg, ControlManager& ctrl, NfcManager& nfc)
  : server(srv), configManager(cfg), controlManager(ctrl), nfcManager(nfc), relayLabelId(0), buzzerLabelId(0), 
    buttonLabelId(0), tagDataLabelId(0), deviceNameId(0), influxHostId(0), influxPortId(0), influxBucketId(0), 
    influxOrgId(0), influxTokenId(0), ntpId(0), debounceDelayId(0), firmwareUrlId(0), firmwareUploadStatusId(0),
    firmwareUploadButtonId(0) {}

void WebServerManager::onSaveSettings(Control* control, int id, void* userData) {
  CallbackData* data = static_cast<CallbackData*>(userData);
  ConfigManager* cm = data->configManager;
  Config& config = cm->get();

  switch (data->id) {
    case 1: // Device Name
      config.device = control->value;
      break;
    case 2: // InfluxDB Host
      config.influxHost = control->value;
      break;
    case 3: // InfluxDB Port
      config.influxPort = control->value.toInt();
      break;
    case 4: // InfluxDB Bucket
      config.influxBucket = control->value;
      break;
    case 5: // InfluxDB Organization
      config.influxOrg = control->value;
      break;
    case 6: // InfluxDB Token
      config.influxToken = control->value;
      break;
    case 7: // NTP Server
      config.ntp = control->value;
      break;
    case 8: // Debounce Delay
      config.debounceDelay = control->value.toInt();
      break;
    case 9: // Firmware URL
      config.firmwareUrl = control->value;
      break;
  }
  cm->save();
}

void WebServerManager::onRelayToggle(Control* control, int id, void* userData) {
  ControlManager* cm = static_cast<ControlManager*>(userData);
  cm->setRelayState(false, true); // Toggle relay
}

void WebServerManager::onBuzzerError(Control* control, int id, void* userData) {
  ControlManager* cm = static_cast<ControlManager*>(userData);
  if (control->value.toInt() == 1) {
    cm->playErrorSound();
  }
}

void WebServerManager::onBuzzerWarning(Control* control, int id, void* userData) {
  ControlManager* cm = static_cast<ControlManager*>(userData);
  if (control->value.toInt() == 1) {
    cm->playWarningSound();
  }
}

void WebServerManager::onBuzzerSuccess(Control* control, int id, void* userData) {
  ControlManager* cm = static_cast<ControlManager*>(userData);
  if (control->value.toInt() == 1) {
    cm->playSuccessSound();
  }
}

void WebServerManager::onWriteTag(Control* control, int id, void* userData) {
  WebServerManager* wm = static_cast<WebServerManager*>(userData);
  if (control->value.toInt() == 1) {
    String text = ESPUI.getControl(control->id - 1)->value; // Text input is previous control
    if (text.isEmpty()) {
      ESPUI.updateLabel(wm->tagDataLabelId, "No data provided to write");
      wm->controlManager.playErrorSound();
    } else {
      wm->nfcManager.setPendingWrite(text);
      ESPUI.updateLabel(wm->tagDataLabelId, "Waiting for NFC tag...");
    }
  }
}

void WebServerManager::onFirmwareUploadButton(Control* control, int id, void* userData) {
  WebServerManager* wm = static_cast<WebServerManager*>(userData);
  if (control->value.toInt() == 1) {
    ESPUI.updateLabel(wm->firmwareUploadStatusId, "Please visit /upload-form to upload firmware");
  }
}

void WebServerManager::handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  static File uploadFile;
  
  if (!index) { // Start of upload
    if (!filename.endsWith(".bin")) {
      ESPUI.updateLabel(firmwareUploadStatusId, "Invalid file type. Please upload a .bin file");
      controlManager.playErrorSound();
      request->send(400, "text/plain", "Invalid file type");
      return;
    }
    if (!SPIFFS.begin()) {
      ESPUI.updateLabel(firmwareUploadStatusId, "Failed to mount SPIFFS");
      controlManager.playErrorSound();
      request->send(500, "text/plain", "Failed to mount SPIFFS");
      return;
    }
    uploadFile = SPIFFS.open("/firmware.bin", "w");
    if (!uploadFile) {
      ESPUI.updateLabel(firmwareUploadStatusId, "Failed to open file for writing");
      controlManager.playErrorSound();
      request->send(500, "text/plain", "Failed to open file");
      SPIFFS.end();
      return;
    }
    ESPUI.updateLabel(firmwareUploadStatusId, "Uploading firmware...");
  }

  if (len) { // Write data chunk
    uploadFile.write(reinterpret_cast<char*>(data), len);
  }

  if (final) { // End of upload
    uploadFile.close();
    SPIFFS.end();
    
    WiFiClient client;
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, "/firmware.bin");
    if (ret == HTTP_UPDATE_OK) {
      ESPUI.updateLabel(firmwareUploadStatusId, "Firmware update successful");
      controlManager.playSuccessSound();
      request->send(200, "text/plain", "Firmware update successful");
    } else {
      ESPUI.updateLabel(firmwareUploadStatusId, "Firmware update failed: " + String(ESPhttpUpdate.getLastErrorString()));
      controlManager.playErrorSound();
      request->send(500, "text/plain", "Firmware update failed: " + ESPhttpUpdate.getLastErrorString());
    }
  }
}

void WebServerManager::updateStatus() {
  DeviceStatus status = controlManager.getStatus();
  ESPUI.updateLabel(relayLabelId, status.relayOn ? "On" : "Off");
  ESPUI.updateLabel(buzzerLabelId, status.buzzerOn ? "On" : "Off");
  ESPUI.updateLabel(buttonLabelId, status.buttonPressed ? "Pressed" : "Released");
  ESPUI.updateLabel(tagDataLabelId, nfcManager.getLastTagData());
}

void WebServerManager::setupUI() {
  Config cfg = configManager.get();
  
  ESPUI.setVerbosity(Verbosity::Quiet);
  ESPUI.begin("ESP8266 Configuration");

  uint16_t tab1 = ESPUI.addControl(ControlType::Tab, "Settings", "Settings");
  uint16_t tab2 = ESPUI.addControl(ControlType::Tab, "Controls", "Controls");
  uint16_t tab3 = ESPUI.addControl(ControlType::Tab, "NFC Tag", "NFC Tag");
  uint16_t tab4 = ESPUI.addControl(ControlType::Tab, "Press Counter", "Press Counter");

  // Settings Tab
  ESPUI.addControl(ControlType::Label, "Device Settings", "Device Settings", ControlColor::Turquoise, tab1);
  
  deviceData = {&configManager, 1};
  deviceNameId = ESPUI.addControl(ControlType::Text, "Device Name", cfg.device, ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &deviceData);
  
  influxHostData = {&configManager, 2};
  influxHostId = ESPUI.addControl(ControlType::Text, "InfluxDB Host", cfg.influxHost, ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &influxHostData);
  
  influxPortData = {&configManager, 3};
  influxPortId = ESPUI.addControl(ControlType::Number, "InfluxDB Port", String(cfg.influxPort), ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &influxPortData);
  
  influxBucketData = {&configManager, 4};
  influxBucketId = ESPUI.addControl(ControlType::Text, "InfluxDB Bucket", cfg.influxBucket, ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &influxBucketData);
  
  influxOrgData = {&configManager, 5};
  influxOrgId = ESPUI.addControl(ControlType::Text, "InfluxDB Organization", cfg.influxOrg, ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &influxOrgData);
  
  influxTokenData = {&configManager, 6};
  influxTokenId = ESPUI.addControl(ControlType::Text, "InfluxDB Token", cfg.influxToken, ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &influxTokenData);
  
  ntpData = {&configManager, 7};
  ntpId = ESPUI.addControl(ControlType::Text, "NTP Server", cfg.ntp, ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &ntpData);
  
  debounceDelayData = {&configManager, 8};
  debounceDelayId = ESPUI.addControl(ControlType::Number, "Debounce Delay", String(cfg.debounceDelay), ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &debounceDelayData);
  
  firmwareUrlData = {&configManager, 9};
  firmwareUrlId = ESPUI.addControl(ControlType::Text, "Firmware URL", cfg.firmwareUrl, ControlColor::Turquoise, tab1, &WebServerManager::onSaveSettings, &firmwareUrlData);

  // Firmware Upload
  ESPUI.addControl(ControlType::Label, "Firmware Upload", "Firmware Upload", ControlColor::Turquoise, tab1);
  firmwareUploadButtonId = ESPUI.addControl(ControlType::Button, "Upload Firmware", "Go to Upload Form", ControlColor::Emerald, tab1, &WebServerManager::onFirmwareUploadButton, this);
  firmwareUploadStatusId = ESPUI.addControl(ControlType::Label, "Firmware Upload Status", "No file uploaded", ControlColor::Alizarin, tab1);

  // Controls Tab
  ESPUI.addControl(ControlType::Label, "Device Controls", "Device Controls", ControlColor::Emerald, tab2);
  ESPUI.addControl(ControlType::Switcher, "Relay Toggle", "0", ControlColor::Emerald, tab2, &WebServerManager::onRelayToggle, &controlManager);
  ESPUI.addControl(ControlType::Button, "Error Sound", "Play", ControlColor::Emerald, tab2, &WebServerManager::onBuzzerError, &controlManager);
  ESPUI.addControl(ControlType::Button, "Warning Sound", "Play", ControlColor::Emerald, tab2, &WebServerManager::onBuzzerWarning, &controlManager);
  ESPUI.addControl(ControlType::Button, "Success Sound", "Play", ControlColor::Emerald, tab2, &WebServerManager::onBuzzerSuccess, &controlManager);
  
  ESPUI.addControl(ControlType::Label, "Device Status", "Device Status", ControlColor::Alizarin, tab2);
  relayLabelId = ESPUI.addControl(ControlType::Label, "Relay Status", "Off", ControlColor::Alizarin, tab2);
  buzzerLabelId = ESPUI.addControl(ControlType::Label, "Buzzer Status", "Off", ControlColor::Alizarin, tab2);
  buttonLabelId = ESPUI.addControl(ControlType::Label, "Button Status", "Released", ControlColor::Alizarin, tab2);

  // NFC Tag Tab
  ESPUI.addControl(ControlType::Label, "NFC Operations", "NFC Operations", ControlColor::Turquoise, tab3);
  uint16_t tagInputId = ESPUI.addControl(ControlType::Text, "Tag Text", "example text", ControlColor::Turquoise, tab3); // Changed label to "Tag Text"
  ESPUI.addControl(ControlType::Button, "Write Tag", "Write", ControlColor::Emerald, tab3, &WebServerManager::onWriteTag, this);
  tagDataLabelId = ESPUI.addControl(ControlType::Label, "Last Tag Data", "No tag read", ControlColor::Alizarin, tab3);

  // Press Counter Tab
  ESPUI.addControl(ControlType::Label, "Press Counter", "Press Counter", ControlColor::Peterriver, tab4);
  
  updateStatus();
}

void WebServerManager::begin() {
  setupUI();
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200);
  }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    this->handleFileUpload(request, filename, index, data, len, final);
  });
  server.on("/upload-form", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><body><h2>Upload Firmware</h2>";
    html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
    html += "<input type='file' name='firmware' accept='.bin'>";
    html += "<input type='submit' value='Upload Firmware'>";
    html += "</form>";
    html += "<p>Return to <a href='/'>ESPUI Interface</a></p>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });
  server.begin();
  Serial.println("Web server started");
}