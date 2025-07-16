#ifndef DEFINES_H
#define DEFINES_H

// Configuration settings for the device and InfluxDB connection
#define DEVICE "hydraulic_press"       // Unique identifier for the device
#define WIFI_SSID "TW factory"         // WiFi network SSID
#define WIFI_PASSWORD "e8cac589tw"     // WiFi network password
#define INFLUXDB_URL "http://192.168.1.37:8086" // InfluxDB server URL
#define INFLUXDB_TOKEN "_2B1s0cJGXznc9rXE41mxOusyn159HAAsMyGlLxiFkZd0V8dJKztKYVEPNeKGQSgc0gulh-vhnhn6ONcCr3lBA==" // InfluxDB authentication token
#define INFLUXDB_ORG "tooswasher"      // InfluxDB organization name
#define INFLUXDB_BUCKET "manufacture"  // InfluxDB bucket for data storage

// RFID tag start characters
#define RFID_UID_START_DIE '1'     // UID starting character for die tags
#define RFID_UID_START_PEOPLE '2'  // UID starting character for people tags

// Hardware and timing configurations
#define INPUT_PIN 2                    // GPIO pin for pulse input
#define RELAY_PIN 16                   // GPIO pin for relay control
#define RC522_SS_PIN 4                 // GPIO pin for RC522 SDA (SPI Slave Select)
#define RC522_RST_PIN 5                // GPIO pin for RC522 RST (Reset)

// Timing constants for pulse detection and data reporting
#define EDGE_TIMEOUT 30                // Timeout for edge detection in milliseconds
#define MIN_PULSE_TRAIN_DURATION 250   // Minimum duration for a valid pulse train in milliseconds
#define REPORT_INTERVAL 1              // Number of pulses before sending to InfluxDB
#define SEND_RETRY_INTERVAL 5000       // Retry interval for failed sends in milliseconds
#define HTTP_REQUEST_TIMEOUT 0.3       // Timeout for asyncHTTPrequest in seconds

// EEPROM settings for persistent storage
#define EEPROM_SIZE 32                 // Size of EEPROM allocation in bytes
#define INIT_MARKER 0xAA               // Marker to indicate initialized EEPROM

#endif