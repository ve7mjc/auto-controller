#include "mqtt.h"

Mqtt::Mqtt()
{
  wclient = new WiFiClient();
  pubsubc = new PubSubClient(*wclient);
}

void Mqtt::loop()
{
  if (WiFi.status() == WL_CONNECTED)
    if (!this->pubsubc->connected())
      this->reconnect();
}

void Mqtt::connectBroker()
{

  
}


void Mqtt::reconnect()
{
  // instead of looping until we are connected
  // we will attempt it once
  // we must call it multiple times
  if (!this->pubsubc->connected()) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect only if we have a wifi connection
      if (this->pubsubc->connect("ESP8266Client")) {
        Serial.println("connected");
        this->pubsubc->publish("outTopic", "hello world");
        this->pubsubc->subscribe("inTopic");
      } else {
        Serial.print("failed, rc=");
        Serial.print(this->pubsubc->state());
      }
    }
  }
}

