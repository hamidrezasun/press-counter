#ifndef INFLUXDB_H
#define INFLUXDB_H

#include <asyncHTTPrequest.h>
#include <ESP8266WiFiMulti.h>
#include "defines.h"
#include "state.h"

// External declarations for global objects used in InfluxDB communication
extern asyncHTTPrequest request;      // HTTP request object for sending data
extern ESP8266WiFiMulti wifiMulti;    // WiFi multi-connection manager

// Function declarations for sending data to InfluxDB
bool sendTo();                       // Sends pulse data to InfluxDB
void sendDeviceInfo();                // Sends device information to InfluxDB

#endif