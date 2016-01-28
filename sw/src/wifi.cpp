#include "wifi.h"
// #include "callback.h"

// Must define static members
bool Wifi::newEventHolding = false;
WiFiEvent_t Wifi::newEvent = WIFI_EVENT_MAX;

Wifi::Wifi(Config* config_p)
 : WiFiClient()
{
  // Pass configuration
  config = config_p;

  // temporary for now
  lastWifiApScanTime = millis();

  defaultAp.ssid = config->defaultApSSid;
  defaultAp.psk = config->defaultApPsk;

  networks[0].ssid = config->networkSsid;
  networks[0].psk = config->networkPsk;

  // Build hostname as {vehicleNameShort}ESP
  hostname = String(config->vehicleNameShort);
  hostname += String("ESP");
  WiFi.hostname(hostname);
  
  // Setting the ESP8266 station to connect to the AP (which is recorded)
  // automatically or not when powered on.
//  WiFi.setAutoConnect(false);
//  WiFi.setAutoReconnect(true);

  // Print MAC Address
//  Serial.print("MAC Address: ");
//  Serial.println(WiFi.macAddress());

  ArduinoOTA.setHostname((const char *)hostname.c_str());

  // Configure MQTT but do not connect
  // For whatever reason, we MUST create a (Client)WiFiClient
  // and pass it into the PubSubClient constructor.  Fix on a
  // rainy day
  // WiFiClient* wclient = new WiFiClient();
  mqtt = new Mqtt(*(Client*)this);
  mqtt->setServer(config->mqttBrokerHost, config->mqttBrokerPort);

}

//------------------------------------------------------------------------
// Callback function.
//typedef CBFunctor1wRet<int, int> cbEvent;

void Wifi::init()
{
  WiFi.onEvent(Wifi::onWifiEvent);
  switchState(STA_READY);
  loop();
}

// WiFiEventCb cbEvent
void Wifi::onWifiEvent(WiFiEvent_t event) {
  Wifi::newEvent = event;
  Wifi::newEventHolding = true;
}

void Wifi::loop()
{

  // process events
  if (newEventHolding) {
    newEventHolding = false;
    switch(newEvent) {
      case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.println("WiFiEvent: WIFI_EVENT_STAMODE_CONNECTED");
      break;
      
      case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("WiFiEvent: WIFI_EVENT_STAMODE_DISCONNECTED");
      switchState(STA_DISCONNECTED);
      break;
      
      case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
      Serial.println("WiFiEvent: WIFI_EVENT_STAMODE_AUTHMODE_CHANGE");
      break;
      
      case WIFI_EVENT_STAMODE_GOT_IP:
      
      Serial.println("WiFiEvent: WIFI_EVENT_STAMODE_GOT_IP");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      if (mqtt->connect("truckesp")) {
        // Note, following content is also hardcoded into Mqtt class
        // reconnect method; This must be improved
        mqtt->publish("clients/blazer/status","online");
        mqtt->subscribe("vccs/blazer/command/#");
      }
      
      // Begin cannot be called more than once
      // as a bool _initialized is checked
      // during entry to the method
      ArduinoOTA.begin();
      
      switchState(STA_CONNECTED);
      
      break;
      
      case WIFI_EVENT_STAMODE_DHCP_TIMEOUT:
      Serial.println("WiFiEvent: WIFI_EVENT_STAMODE_DHCP_TIMEOUT");
      break;
      
      case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      Serial.println("WiFiEvent: WIFI_EVENT_SOFTAPMODE_STACONNECTED");
      break;
      
      case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      Serial.println("WiFiEvent: WIFI_EVENT_SOFTAPMODE_STADISCONNECTED");
      break;
      
      case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
      Serial.println("WiFiEvent: WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED");
      break;
      
      case WIFI_EVENT_MAX:
      Serial.println("WiFiEvent: WIFI_EVENT_MAX");
      break;
      
      default:
      break;
    }
  }

  
  switch (state.state) {
    
    case STA_READY:

      WiFi.disconnect(true); // clear config ! big deal!

      if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
        delay(100); // todo, this is blocking for 100 ms !!
      }

      Serial.print("Connecting to SSID ");
      Serial.print(networks[0].ssid);
      Serial.print(" psk: ");
      Serial.println(networks[0].psk);

      WiFi.begin(networks[0].ssid.c_str(), networks[0].psk.c_str());      

      switchState(STA_CONNECTING);
      
      break;
      
    case STA_CONNECTING:
    
      break;
        
//      // State: We have been unable to connect and it has been 5 seconds
//      if ((WiFi.status() != WL_CONNECTED) && (state.stageTime > 10000)) {
//
//        printStatus();
//
//        // We are going to switch to Access Point Mode
//        switchState(AP);
//
//        // ... Uncomment this for debugging output.
//        WiFi.printDiag(Serial);
//        
//        // we are expired on the connect
//        Serial.println("STA is not connected; Timing out..");
//        Serial.println("Going into AP mode");
//
//        WiFi.mode(WIFI_AP);
//        delay(10);
//        WiFi.softAP(this->defaultAp.ssid.c_str(), this->defaultAp.psk.c_str());
//    
//        Serial.print("IP address: ");
//        Serial.println(WiFi.softAPIP());
//
//      }

    case STA_CONNECTED:
    
      ArduinoOTA.handle();
    
      break;

    case STA_DISCONNECTED:

      switchState(STA_READY);
      break;

    case AP:

      // Access Point
      // Look for a better CLIENT connection
      // every 5 seconds
      
      if ((millis() - lastWifiApScanTime) > 5000) {

        int n = WiFi.scanNetworks();

        for (int i = 0; i < n; ++i) {
          if (WiFi.SSID(i) == config->networkSsid) {
            Serial.println("Located preferred AP; Switching to STA from AP mode.");
            Serial.print("RSSI: ");
            Serial.print(WiFi.RSSI(i));
            Serial.println(" dBm");
            switchState(STA_READY);
          } else {
            // Troubleshooting
            // Print SSID and RSSI for each network found
//            Serial.print(i + 1);
//            Serial.print(": ");
//            Serial.print(WiFi.SSID(i));
//            Serial.print(" (");
//            Serial.print(WiFi.RSSI(i));
//            Serial.print(")");
//            Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
          }
          
        }
      
        lastWifiApScanTime = millis();
      }

      break;
    
  }

  // track elapsed time in particular state
  state.stageTime = millis() - state.stageStartTime;

  // tend to PubSubClient networking chores
  mqtt->loop();
  

}

void Wifi::printStatus()
{
  Serial.print("WiFi.status == ");
  switch(WiFi.status()) {
    case WL_CONNECTED:
    Serial.println("STATION_GOT_IP");
    break;
    case WL_NO_SSID_AVAIL:
    Serial.println("STATION_NO_AP_FOUND");
    break;
    case WL_CONNECT_FAILED:
    Serial.println("STATION_CONNECT_FAIL or STATION_WRONG_PASSWORD");
    break;
    case WL_IDLE_STATUS:
    Serial.println("STATION_IDLE");
    break;
    case WL_DISCONNECTED:
    Serial.println("WL_DISCONNECTED");
    break;
    default:
    Serial.println("UNKNOWN");
  }
  
}

void Wifi::switchState(State state_new)
{
  state.state = state_new;
  state.stageTime = 0;
  state.stageStartTime = millis();
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
