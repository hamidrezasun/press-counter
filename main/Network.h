#ifndef NETWORK_H
#define NETWORK_H

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include "Config.h"

class PressCounter; // Forward declaration
class Network {
  private:
    WiFiUDP ntpUDP;
    NTPClient* timeClient;
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    Config* config;
    static Network* instance; // Static instance for callback
    unsigned long currentTime = 0;//for isConnected interval

  public:
    void setup(Config& conf);
    bool connectAll();
    bool sendDeviceInfo();
    unsigned long getTime();
    void loop();
    bool isConnected();
    PubSubClient& getMqttClient();
    void sendCountSignal(int press_count);
    void sendButtonSignal(const String& signal); // New method for button signals
    static void callback(char* topic, byte* payload, unsigned int length);
    static void setInstance(Network* inst) { instance = inst; }
};

#endif