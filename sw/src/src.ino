#include "config.h"
#include "vehicle.h"

// Adafruit Feather Huzzah Specifics
#define RED_LED 0
#define BLUE_LED 2

Config* config;
Vehicle* vehicle;

// #######################
// SETUP
// #######################
void setup()
{
  // Configure and turn off Blue LED at
  // wifi module
//  pinMode(BLUE_LED, OUTPUT);
//  digitalWrite(BLUE_LED, LOW);

  config = new Config();
  vehicle = new Vehicle(config);

  Serial.begin(115200);

  // Initialize file system.
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  vehicle->init();
  
}

// for periodic tasks
long lastCheck = 0;

bool lowPower = false;

void loop()
{

  // 20 hz loop
 
  if ((millis() - lastCheck > 50) || lowPower) {
    yield();
    vehicle->loop();
    
    lastCheck = millis();

    if (lowPower) {
      ESP.deepSleep(5000, WAKE_RF_DEFAULT);   // 30 000 000
      delay(1000);
    }
  }
  

}
