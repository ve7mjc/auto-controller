

#include <ESP.h>

// #define OTA_DEBUG

#include "config.h"
#include "vehicle.h"

// Adafruit Feather Huzzah Specifics
const uint8_t RED_LED = 0;
const uint8_t BLUE_LED = 2;

Config* config;
Vehicle* vehicle;

void setup()
{

  // Load config
  config = new Config();
  
  // 115200 is optimal speed of bootloader
  Serial.begin(115200);

  // Print reset cause and information
  Serial.print("\r\n####\r\nReset Reason: ");
  Serial.println(ESP.getResetReason());
  Serial.print("Reset Info: ");
  Serial.println(ESP.getResetInfo());
  Serial.println("####");

  // Initialize flash file system.
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  vehicle = new Vehicle(config);
  vehicle->init();

}

// for periodic tasks
long lastCheck = 0;

void loop()
{

  // 1 kHz loop (once per ms)
  if ((millis() > lastCheck) || vehicle->lowPowerMode) {

    // tend to ESP/Wifi chores
    yield();
    
    vehicle->loop();
    
    lastCheck = millis();

    // Low power mode is very experimental at the moment
    // Deep sleep is essentially shutting down the micro
    // as resuming from deep sleep results in a reset and
    // thus calls setup() again.
    if (vehicle->lowPowerMode) {
      ESP.deepSleep(5000, WAKE_RF_DEFAULT);   // 30 000 000
      delay(1000);
    }
  }
  
}

//  File configFile = SPIFFS.open("/cl_conf.txt", "r");
//  if (!configFile)
//  {
//    Serial.println("Failed to open cl_conf.txt.");
//  } else {
//    // Read content from config file.
//    String content = configFile.readString();
//    configFile.close();
//    Serial.println("/cl_conf.txt contents:");
//    Serial.print(content);
//  }
