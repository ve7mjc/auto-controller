#ifndef MQTT_H
#define MQTT_H

// todo, inherit class versus creating instance and pointer
//

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>

class Mqtt 
{

  public:
    Mqtt();
    void loop();
    
    PubSubClient* pubsubc;
    void connectBroker();
    void reconnect();

  private:
    WiFiClient* wclient;
  
};


#endif
