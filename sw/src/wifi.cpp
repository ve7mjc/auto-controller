#include "wifi.h"

Wifi::Wifi(Config* config_p)
{
  // Pass configuration
  config = config_p;

  // temporary for now
  lastWifiApScanTime = millis();

  defaultAp.ssid = config->defaultApSSid;
  defaultAp.psk = config->defaultApPsk;

  this->networks[0].ssid = config->networkSsid;
  this->networks[0].psk = config->networkPsk;

  this->hostname = String("TruckESP");
  WiFi.hostname(this->hostname);

  ArduinoOTA.setHostname((const char *)this->hostname.c_str());

  // Configure MQTT but do not connect
  this->mqttc = new Mqtt();
  this->mqttc->pubsubc->setServer(config->mqttBrokerHost, config->mqttBrokerPort);
  this->mqttc->pubsubc->setCallback(mqttCallback);

}

void Wifi::init()
{
  switchState(STA_READY);
  loop();
}

void Wifi::loop()
{
  switch (this->state.state) {
    
    case STA_READY:

      if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
        delay(10); // todo, this is blocking for 10 ms !!
      }
      
      WiFi.begin(this->networks[0].ssid.c_str(), this->networks[0].psk.c_str());

      switchState(STA_CONNECTING);
      
      break;
      
    case STA_CONNECTING:

      // If we are connected and have been for in excess of 10 seconds
      // There must be a better way to do this such as determining what
      // it is we want -- an IP address?  If so, check for an IP address
      // in the same loop and thus we can connect much faster
      if ((WiFi.status() == WL_CONNECTED) && (state.stageTime > 2000) && WiFi.localIP()) {

        // TODO: Much more checking for a PROPER connect
        // Do we have a valid dhcp lease?
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Initialize Over-the-air (OTA) firmware loading services
        ArduinoOTA.begin();
        mqttc->pubsubc->connect("truckesp");
        
        switchState(STA_CONNECTED);
        
      } else {
        if (WiFi.status() == WL_CONNECTED) {
          // Serial.println(WiFi.localIP());
        }
      }

      // State: We have been unable to connect and it has been 5 seconds
      if ((WiFi.status() != WL_CONNECTED) && ((millis() - state.stageTime) > 5000)) {

        // We are going to switch to Access Point Mode
        switchState(AP);

        // ... Uncomment this for debugging output.
        //WiFi.printDiag(Serial);
        
        // we are expired on the connect
        Serial.println("timeout connecting");
        Serial.println("Going into AP mode");

        WiFi.mode(WIFI_AP);
        delay(10);
        WiFi.softAP(this->defaultAp.ssid.c_str(), this->defaultAp.psk.c_str());
    
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());

      }
      
      break;
      
    case STA_CONNECTED:
    
      // check for disconnect
      if(WiFi.status() != WL_CONNECTED) {

        // Return back to STA_READY as if we just restarted
        switchState(STA_READY);
        
      } else {
        ArduinoOTA.handle();
      }
      
      break;

    case AP:

      // Access Point
      // Look for a better CLIENT connection
      // every 5 seconds
      
      if ((millis() - lastWifiApScanTime) > 5000) {

        int n = WiFi.scanNetworks();
        bool found = false;
        for (int i = 0; i < n; ++i) {
          if (WiFi.SSID(i) == config->networkSsid) {
            found = true;
            
            // Return back to STA_READY as if we just restarted
            switchState(STA_READY);
          }

          // Troubleshooting
          // Print SSID and RSSI for each network found
          Serial.print(i + 1);
          Serial.print(": ");
          Serial.print(WiFi.SSID(i));
          Serial.print(" (");
          Serial.print(WiFi.RSSI(i));
          Serial.print(")");
          Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
          
        }
      
        lastWifiApScanTime = millis();
      }

      break;
    
  }

  // track elapsed time in particular state
  this->state.stageTime = millis() - this->state.stageStartTime;

  // MQTT
  if (state.state == STA_CONNECTED) {
    mqttc->loop();
  }


}

void Wifi::switchState(State state)
{

  switch (state) {
    case STA_READY:
    Serial.println("\r\nReady to connect to STA");
    break;
  }
  
  this->state.state = state;
  this->state.stageTime = 0;
  this->state.stageStartTime = millis();
}

State Wifi::checkState() 
{
  return this->state.state;
}

void Wifi::mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    //digitalWrite(RED_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    //digitalWrite(RED_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

/**
 * @brief Read WiFi connection information from file system.
 * @param ssid String pointer for storing SSID.
 * @param pass String pointer for storing PSK.
 * @return True or False.
 * 
 * The config file have to containt the WiFi SSID in the first line
 * and the WiFi PSK in the second line.
 * Line seperator can be \r\n (CR LF) \r or \n.
 */
bool Wifi::loadConfig(String *ssid, String *pass)
{
  // open file for reading.
  File configFile = SPIFFS.open("/cl_conf.txt", "r");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt.");

    return false;
  }

  // Read content from config file.
  String content = configFile.readString();
  configFile.close();
  
  content.trim();

  // Check if ther is a second line available.
  int8_t pos = content.indexOf("\r\n");
  uint8_t le = 2;
  // check for linux and mac line ending.
  if (pos == -1)
  {
    le = 1;
    pos = content.indexOf("\n");
    if (pos == -1)
    {
      pos = content.indexOf("\r");
    }
  }

  // If there is no second line: Some information is missing.
  if (pos == -1)
  {
    Serial.println("Invalid content.");
    Serial.println(content);

    return false;
  }

  // Store SSID and PSK into string vars.
  *ssid = content.substring(0, pos);
  *pass = content.substring(pos + le);
  
  ssid->trim();
  pass->trim();

#ifdef SERIAL_VERBOSE
  Serial.println("----- file content -----");
  Serial.println(content);
  Serial.println("----- file content -----");
  Serial.println("ssid: " + *ssid);
  Serial.println("psk:  " + *pass);
#endif

  return true;
} // loadConfig

/**
 * @brief Save WiFi SSID and PSK to configuration file.
 * @param ssid SSID as string pointer.
 * @param pass PSK as string pointer,
 * @return True or False.
 */
bool Wifi::saveConfig(String *ssid, String *pass)
{
  // Open config file for writing.
  File configFile = SPIFFS.open("/cl_conf.txt", "w");
  if (!configFile)
  {
    Serial.println("Failed to open cl_conf.txt for writing");

    return false;
  }

  // Save SSID and PSK.
  configFile.println(*ssid);
  configFile.println(*pass);

  configFile.close();
  
  return true;
} // saveConfig

void Wifi::printEncryptionType(int thisType) {
  // read the encryption type and print out the name:
  switch (thisType) {
    case ENC_TYPE_WEP:
      Serial.println("WEP");
      break;
    case ENC_TYPE_TKIP:
      Serial.println("WPA");
      break;
    case ENC_TYPE_CCMP:
      Serial.println("WPA2");
      break;
    case ENC_TYPE_NONE:
      Serial.println("None");
      break;
    case ENC_TYPE_AUTO:
      Serial.println("Auto");
      break;
  }
}

void Wifi::listNetworks() {
  // scan for nearby networks:
  Serial.println("** Scan Networks **");
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    Serial.println("Couldn't get a wifi connection or no wifi in range");
    while (true);
  }

  // print the list of networks seen:
  Serial.print("number of available networks:");
  Serial.println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") ");
    Serial.print(WiFi.SSID(thisNet));
    Serial.print("\tSignal: ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.print("\tEncryption: ");
    printEncryptionType(WiFi.encryptionType(thisNet));
  }
}
