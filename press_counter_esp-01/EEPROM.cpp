#include <EEPROM.h>
#include "EEPROM.h"

// Saves pulse counts, max send duration, and die ID to EEPROM if changed
void saveCountsToEEPROM() {
  static uint32_t lastSavedPulseCount = 0;    // Last saved pulse count
  static uint32_t lastSavedFailedCount = 0;   // Last saved failed count
  static uint32_t lastSavedMaxDuration = 0;   // Last saved max send duration
  static String lastSavedDieID = "";          // Last saved die ID

  // Check if any values have changed
  if (state.pulseCount != lastSavedPulseCount || 
      state.failedPulseCount != lastSavedFailedCount ||
      state.maxSendDuration_ms != lastSavedMaxDuration ||
      state.currentDieID != lastSavedDieID) {
    
    EEPROM.begin(EEPROM_SIZE); // Initialize EEPROM
    EEPROM.write(0, INIT_MARKER); // Write initialization marker
    EEPROM.put(1, state.pulseCount); // Save pulse count
    EEPROM.put(5, state.failedPulseCount); // Save failed count
    EEPROM.put(9, state.maxSendDuration_ms); // Save max send duration
    // Save die ID as a null-terminated string (max 15 chars + null)
    int len = min((int)state.currentDieID.length(), 15); // Limit to 15 chars, cast to int
    for (int i = 0; i < 16; i++) {
      if (i < len) {
        EEPROM.write(13 + i, state.currentDieID[i]);
      } else {
        EEPROM.write(13 + i, 0); // Clear remaining bytes
      }
    }
    EEPROM.commit(); // Commit changes
    EEPROM.end(); // Release EEPROM
    
    // Update last saved values
    lastSavedPulseCount = state.pulseCount;
    lastSavedFailedCount = state.failedPulseCount;
    lastSavedMaxDuration = state.maxSendDuration_ms;
    lastSavedDieID = state.currentDieID;
  }
}

// Loads pulse counts, max send duration, and die ID from EEPROM
void loadCountsFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE); // Initialize EEPROM
  if (EEPROM.read(0) != INIT_MARKER) { // Check for initialization marker
    EEPROM.write(0, INIT_MARKER); // Write marker if uninitialized
    EEPROM.put(1, 0); // Initialize pulse count
    EEPROM.put(5, 0); // Initialize failed count
    EEPROM.put(9, 0); // Initialize max send duration
    for (int i = 13; i < 29; i++) {
      EEPROM.write(i, 0); // Clear die ID
    }
    EEPROM.commit(); // Commit changes
    state.pulseCount = 0;
    state.failedPulseCount = 0;
    state.maxSendDuration_ms = 0;
    state.currentDieID = "";
  } else {
    // Load values from EEPROM
    EEPROM.get(1, state.pulseCount);
    EEPROM.get(5, state.failedPulseCount);
    EEPROM.get(9, state.maxSendDuration_ms);
    // Load die ID
    char dieID[16] = {0};
    for (int i = 0; i < 15; i++) { // Read up to 15 chars
      dieID[i] = EEPROM.read(13 + i);
      if (dieID[i] == 0) break; // Stop at null
    }
    state.currentDieID = String(dieID);
  }
  EEPROM.end(); // Release EEPROM
}

// Resets pulse counts, max send duration, and die ID, saving to EEPROM
void resetEEPROM() {
  state.pulseCount = 0;
  state.failedPulseCount = 0;
  state.maxSendDuration_ms = 0;
  state.currentDieID = "";
  saveCountsToEEPROM(); // Save reset values
}