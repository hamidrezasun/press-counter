#ifndef EEPROM_H
#define EEPROM_H

#include "state.h"
#include "defines.h"

// Function declarations for EEPROM operations
void saveCountsToEEPROM();
void loadCountsFromEEPROM();
void resetEEPROM();

#endif