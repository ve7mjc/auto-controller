/* Wrapper for MQTT Client Library to make it more useful
 *  Matthew Currie VE7MJC
 */

#ifndef MQTT_H
#define MQTT_H

#include <QueueList.h>

#include <PubSubClient.h>

// Evaluate which of these must be here
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

enum DebugLevel {
  L_DEBUG,
  L_WARN,
  L_INFO
};

class MqttMessage 
{
  public:
    MqttMessage(char* topic_p, byte* payload_p, unsigned int length) 
    {
      topic = String(topic_p);
      for (int i = 0; i < length; i++) {
        payload += (char)payload_p[i];
      }
    }
    String topic;
    String payload;
};


class Mqtt: public PubSubClient
{

  public:
    
    Mqtt();
    Mqtt(Client& client);

    void loop();
    boolean publish(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained);
    boolean publish(const char* topic, const uint8_t* payload, unsigned int plength);
    boolean publish(const char* topic, const char* payload, boolean retained);
    boolean publish(const char* topic, const char* payload);

    static void onMqttMessage(char* topic, byte* payload, unsigned int length);
    MqttMessage* nextMessage();
    int messagesWaiting();

    void writeLog(const char* message, DebugLevel level = L_DEBUG);
    
    // New Functionality
    static QueueList<MqttMessage*> messageQueue;

  private:

    WiFiClient* wclient;
    const char* logTopic;
    bool reconnect();
    long lastReconnectAttempt;
  
};


#endif
