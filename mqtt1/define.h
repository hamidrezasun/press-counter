#ifndef DEFINE_H
#define DEFINE_H

const int BUTTON_PIN = 2;              // GPIO 2 (D4 on NodeMCU)
const int RC522_SS_PIN = 4;            // GPIO 4 (D2 on NodeMCU) for RC522 SDA
const int RC522_RST_PIN = 5;           // GPIO 5 (D1 on NodeMCU) for RC522 RST
const int RELAY_PIN = 16;              // GPIO 16 (D0 on NodeMCU) for relay
const unsigned long DEBOUNCE_DELAY = 50; // Debounce time in milliseconds
char WIFI_SSID[32] = "";               // WiFi SSID
char WIFI_PASSWORD[64] = "";           // WiFi password
char MQTT_SERVER[64] = "";             // MQTT broker address
int MQTT_PORT = 0;                     // MQTT port
char MQTT_CLIENT_ID[32] = "";          // MQTT client ID
char MQTT_BUTTON_TOPIC[64] = "";       // MQTT topic for button press count
char MQTT_RFID_TOPIC[64] = "";         // MQTT topic for RFID read data
char MQTT_RFID_WRITE_TOPIC[64] = "";   // MQTT topic for RFID write data
char MQTT_DEVICE_INFO_TOPIC[64] = "";  // MQTT topic for device info
unsigned long MQTT_RECONNECT_INTERVAL = 0; // Retry interval for MQTT connection (ms)
unsigned long WIFI_RECONNECT_INTERVAL = 0; // Retry interval for WiFi connection (ms)
const char* BUTTON_COUNT_FILE = "/button_counts.txt"; // File to store button counts
const char* RFID_COUNT_FILE = "/rfid_counts.txt";    // File to store RFID read data
unsigned long RFID_CHECK_INTERVAL = 0; // RFID check interval (ms)
char FIRMWARE_VERSION[32] = "1.0.0";   // Current firmware version
const char* FIRMWARE_SERVER = "http://your-firmware-server.com"; // Firmware server address
const char* FIRMWARE_VERSION_URL = "http://your-firmware-server.com/update"; // Version endpoint

#endif