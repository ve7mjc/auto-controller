#include "mqtt.h"

Mqtt::Mqtt()
 : PubSubClient()
{
  lastReconnectAttempt = 0;
  setCallback(Mqtt::onMqttMessage);
}

Mqtt::Mqtt(Client& client) 
  : PubSubClient(client)
{
  lastReconnectAttempt = 0; 
  setCallback(Mqtt::onMqttMessage);
}

QueueList<MqttMessage*> Mqtt::messageQueue;

void Mqtt::onMqttMessage(char* topic, byte* payload, unsigned int length)
{
  MqttMessage* msg = new MqttMessage(topic, payload, length);
  messageQueue.push(msg);
}

MqttMessage* Mqtt::nextMessage()
{
  return messageQueue.pop();
}

int Mqtt::messagesWaiting()
{
  return messageQueue.count();
}

boolean Mqtt::publish(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained)
{
  PubSubClient::publish(topic, payload, plength, retained);
}

boolean Mqtt::publish(const char* topic, const uint8_t* payload, unsigned int plength)
{
  PubSubClient::publish(topic, payload, plength);
}

boolean Mqtt::publish(const char* topic, const char* payload, boolean retained)
{
  PubSubClient::publish(topic, payload, retained);
}

boolean Mqtt::publish(const char* topic, const char* payload)
{
  PubSubClient::publish(topic, payload);
}

void Mqtt::writeLog(const char* message, DebugLevel level)
{
  // todo: optimize me
  String topic = String("log/" + String("blazer") + "/debug");
  publish(topic.c_str(), message);
}

void Mqtt::loop()
{

  if (connected()) {
    PubSubClient::loop();

    // Process message traffic without callback
    // 

  }
  else {
    // Attempt to reconnect every 2 seconds
    //
    long now = millis();
    if (now - lastReconnectAttempt > 2000) {
      lastReconnectAttempt = now;
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } 


  
}

bool Mqtt::reconnect()
{

  // remove the following hardcoded values
  // Instead manage an array (queulist?) of subscriptions
  // and onConnect messages
  if (connect("truckesp")) {
    publish("clients/blazer/status","online");
    subscribe("vccs/blazer/command/#");
  }

  return connected();

}





