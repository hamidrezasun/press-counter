#ifndef STATE_H
#define STATE_H

#include <Arduino.h> // For String type

// State structure to hold device status, pulse counting, and RFID data
struct State {
  uint32_t pulseCount;            // Number of valid pulses counted
  uint32_t failedPulseCount;      // Number of pulses failed to send
  uint32_t maxSendDuration_ms;    // Maximum duration for sending data to InfluxDB
  unsigned long lastPulseDuration; // Duration of the last valid pulse
  unsigned long lastSendDuration_ms; // Duration of the last send attempt
  unsigned long lastShortPulseDuration; // Duration of the last short pulse
  unsigned long lastShortPulseTime; // Timestamp of the last short pulse
  unsigned long lastEdgeTime;      // Timestamp of the last edge detection
  unsigned long pulseTrainStart;   // Start timestamp of the current pulse train
  unsigned long lastSendAttempt;   // Timestamp of the last send attempt
  bool inPulseTrain;              // Flag indicating active pulse train
  bool shouldSend;                // Flag to trigger data send to InfluxDB
  bool deviceInfoSent;            // Flag indicating if device info was sent
  bool requestInProgress;         // Flag indicating active HTTP request
  bool isDeviceInfoRequest;       // Flag indicating if request is for device info
  unsigned long requestStartTime;  // Start timestamp of the current request
  bool relayOn;                   // Relay status (true = on, false = off)
  bool rfidConnected;             // RFID module connection status
  String lastUID;                 // Last read RFID card UID
  String lastTagData;             // Last read RFID tag data
  bool attemptToWrite;            // Flag indicating attempt to write to RFID
  String newTagData;              // Data to write to RFID tag
  String currentDieID;            // Current die ID stored in EEPROM
  bool isRFIDRequest;             // Flag indicating if request is for RFID data
  unsigned long lastTagReadTime;  // Timestamp of the last tag read for debouncing
};

// External declaration of the global state variable
extern State state;

#endif