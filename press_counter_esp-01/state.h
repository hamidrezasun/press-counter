#ifndef STATE_H
#define STATE_H

// State structure to hold device status and pulse counting data
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
};

// External declaration of the global state variable
extern State state;

#endif