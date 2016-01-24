#ifndef WIFI_H
#define WIFI_H

#include "config.h"
#include "mqtt.h"
#include "Arduino.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <ArduinoOTA.h>

#define HOSTNAME "ESP8266-OTA-" ///< Hostename. The setup function adds the Chip ID at the end.

/// Uncomment the next line for verbose output over UART.
//#define SERIAL_VERBOSE

// we are:
// STA_SEARCHING
// STA_CONNECTING
// STA_CONNECTED
// AP

// Wifi State Machine
enum State { STA_READY, STA_SEARCHING, STA_CONNECTING, STA_CONNECTED, AP };
struct WifiState {
  State state;
  unsigned long stageTime;
  unsigned long stageStartTime;
};

struct Network {
  String ssid;
  String psk;
};

class Wifi
{

  public:
    Wifi(Config* config_p);

    void goAccessPoint();
    void searchNetworks();
    State checkState();
    void switchState(State state);

    bool loadConfig(String *ssid, String *pass);
    bool saveConfig(String *ssid, String *pass);
    void printEncryptionType(int thisType);
    void listNetworks();

    Network networks[10];
    Network defaultAp;
    uint8_t networksNum;
    unsigned long lastWifiApScanTime;
    
    void init();
    void loop();

    String hostname;
    WifiState state;

    Mqtt *mqttc;
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

  private:

    Config* config;

//    const char* ap_default_ssid;
//    const char* ap_default_psk;
    
  
};

#endif
