#include "vehicle.h"

//
// OneWire and DS18B20 was disabled
// out of suspicion that on reboot,
// ESP will enter bootloader mode if anything
// is communicating on the one-wire!
// check battery voltage every 1 second
// ### There are no spare GPIO on the ESP
// that are not used for a bootloader or other
// critical input.  Every peripheral must be
// either I2C or SPI.  Fact of life.
//

// Initialize static members or bear the wrath
bool Vehicle::reportDue = false;

Vehicle::Vehicle(Config* config_p)
{
  
  // Pass configuration in
  config = config_p;
 
  wifi = new Wifi(config);

  //oneWire = new OneWire(2);
  //sensors = new DallasTemperature(oneWire);
  mcp = new Adafruit_MCP23017();

  lowPowerMode = false;
  lastSensorCheck = 0;

  // Time AUX power will remain on after needed in ms
  auxPowerMode = AUX_AUTO;
  powerAuxOffTimeLength =  60UL * 60UL * 1000UL; // 60 minutes
  powerAuxOffTimeLengthSoft = 5UL * 60UL * 1000UL; // 2 minutes
  powerAuxInSoftShutdown = false;
  statusOutputPowerAux = false;
  statusOutputPowerAuxConditions = false;

  // assume for the time being that the accessory input
  // is currently off
  statusInputAccessory = false;

  // Ticker Timers
  reportTimer.attach(1.0, report);

}

void Vehicle::init()
{

  // initializing networking subsystem
  wifi->init();
  
  // OneWire Bus
  // this->sensors->begin();

  // MCP23017 I2C IO Expander
  mcp->begin();

  // Initialize IO tracking
  // Default to IO disabled input with pullups
  // this is the safest default mode
  for(int i = 0; i < NUM_IO; i++) {
    io[i].direction = DIR_INPUT;
    io[i].enabled = false;
    io[i].inverted = false;
    io[i].pullup = true;
    io[i].logic_state = false;
    io[i].bounce_count = 0;
  }

  // Inverted inputs are provided to normalize the logic
  // state in code to represent the physical condition
  // For example, open collector and other IO are often
  // inverted

  // Configure appropriate IO to typical
  // vehicle functions
  io[MCP_IN_ACC].enabled = true;
  io[MCP_IN_ACC].inverted = true;
  
  io[MCP_IN_RESP_SBC_SHUTDOWN].enabled = true;
  io[MCP_IN_RESP_SBC_SHUTDOWN].inverted = true;

  // MCP 12 appears to be damaged or wired incorrectly
  io[MCP_IN_OEM_ALARM_DISARMING].enabled = false;
  io[MCP_IN_OEM_ALARM_DISARMING].inverted = false;
  
  io[MCP_IN_DOORS_LOCKING].enabled = true;
  io[MCP_IN_DOORS_LOCKING].inverted = false;
  
  io[MCP_IN_DOORS_UNLOCKING].enabled = true;
  io[MCP_IN_DOORS_UNLOCKING].inverted = false;
  
  io[MCP_IN_DOOR_PIN].enabled = true;
  io[MCP_IN_DOOR_PIN].inverted = false;
  
  io[MCP_OUT_PWR_AUX].direction = DIR_OUTPUT;
  io[MCP_OUT_PWR_AUX].enabled = true;

  io[MCP_OUT_REQ_AUX_SHUTDOWN].direction = DIR_OUTPUT;
  io[MCP_OUT_REQ_AUX_SHUTDOWN].enabled = true;

  // Configure MCP and mirror current
  // Input bank status
  for( int i = 0; i < NUM_IO; i++ ) {
    if (io[i].direction == DIR_OUTPUT) {
      mcp->pinMode(i, OUTPUT);
      if (io[i].enabled)
        mcp->digitalWrite(i, LOW);
    } else {
      mcp->pinMode(i, INPUT);
      mcp->pullUp(i, io[i].pullup);
    }
  }

  // Read in current state of IO enabled
  // and configured as an input to prime
  // the de-bounce filter
  // Set disabled inputs to FALSE (they will
  // be read as false later also)
  Serial.println("\r\nCurrent IO States:");
  for ( int i = 0; i < NUM_IO; i++ ) {
    
    if (io[i].direction == DIR_INPUT) {
      
      if (io[i].enabled) {
        io[i].logic_state = (io[i].inverted ^ mcp->digitalRead(i));
      }
      else {
        io[i].logic_state = false;
      }

      // Debugging Output

      Serial.print("io[");
      Serial.print(i);
      Serial.print("] state:");
      Serial.print(io[i].logic_state);
      if (!io[i].enabled) Serial.print(" [disabled]");
      Serial.print("\r\n");
    }

  }


}

void Vehicle::setAuxPower(bool value, bool soft)
{
  if (value) {
    powerAuxInSoftShutdown = false;
    statusOutputPowerAux = true;
    mcp->digitalWrite(MCP_OUT_REQ_AUX_SHUTDOWN, LOW);
    mcp->digitalWrite(MCP_OUT_PWR_AUX, HIGH);
  }
  else {
    if (soft) {
      mcp->digitalWrite(MCP_OUT_REQ_AUX_SHUTDOWN, HIGH);
      powerAuxOffTimeHard = millis() + powerAuxOffTimeLengthSoft;
      powerAuxInSoftShutdown = true;
    }
    else {
      statusOutputPowerAux = false;
      mcp->digitalWrite(MCP_OUT_REQ_AUX_SHUTDOWN, HIGH);
      mcp->digitalWrite(MCP_OUT_PWR_AUX, LOW);
      powerAuxInSoftShutdown = false;
    }
  }
}

void Vehicle::loop()
{

  // TODO: check this loop for compatibility or suitability with 1 kHz
  wifi->loop(); // ESP8266Wifi housekeeping

  // Check for MQTT Messages
  while(wifi->mqtt->messagesWaiting()) {
    MqttMessage *msg = wifi->mqtt->nextMessage();
    if (msg->topic == "vccs/blazer/command/setAuxMode") {
      Serial.print("setAuxMode = ");
      Serial.println(msg->payload);
      if (msg->payload.equalsIgnoreCase("off")) auxPowerMode = AUX_OFF;
      if (msg->payload.equalsIgnoreCase("on")) auxPowerMode = AUX_ON;
      if (msg->payload.equalsIgnoreCase("auto")) auxPowerMode = AUX_AUTO;
    }
    delete msg;
  }

  // perform sensor checks per 10 ms (100 Hz)
  // lastSensorCheck is set to 0 in constructor
  if ((millis() - lastSensorCheck) > 10) {
    checkSensors();
    lastSensorCheck = millis();
  }

  // A report is deemed due.  This is set in a Ticker
  // timer callback
  if (reportDue) {
    
    reportDue = false;

    // Construct and send report only if we have both a wifi connection
    // and an MQTT connection
    if ((WiFi.status() == WL_CONNECTED) && wifi->mqtt->connected()) {

      String topic;
      String payload;

      // IO Status
//      payload = "{ ";
//      for ( int i = 0; i < NUM_IO; i++ ) {
//        payload += String(i);
//        payload += " : ";
//        if (io[i].logic_state) payload += "1";
//        else payload += "0";
//        if (i < (NUM_IO - 1)) payload += " , ";
//      }
//      payload += " }";
//      wifi->mqtt->publish(String("vehicles/" + String(config->vehicleNameShort) + "/io").c_str(), payload.c_str());
      
      // Voltage, Supply (usually battery)
      payload = String(voltageSupply, 1);
      payload.toCharArray(buff, 10);
      topic = "vehicles/";
      topic += String(config->vehicleNameShort);
      topic += "/voltage";
      wifi->mqtt->publish(topic.c_str(), buff);

      // Temperature, Onboard
//      payload = String(sensors->getTempCByIndex(0), 1);
//      payload.toCharArray(buff, 10);
//      wifi->mqttc->pubsubc->publish("vehicles/blazer/temperature", buff);
      
      // Acc Input Status
      if (io[MCP_IN_ACC].enabled) {
        topic = "vehicles/";
        topic += String(config->vehicleNameShort);
        topic += "/input-acc/status";
        if (io[MCP_IN_ACC].logic_state)
          wifi->mqtt->publish(topic.c_str(), "on");
        else
          wifi->mqtt->publish(topic.c_str(), "off");
      }

      // Power Status, AUX
      if (statusOutputPowerAux) {
        topic = "vehicles/";
        topic += String(config->vehicleNameShort);
        topic += "/input-acc/status";
        wifi->mqtt->publish("vehicles/blazer/aux-power/status", "on");

        // Power Status, AUX - Time Remaining
        payload =  String(powerAuxTimeUntilOffSecs);
        payload.toCharArray(buff, 10);
        wifi->mqtt->publish("vehicles/blazer/aux-power/time-remaining", buff);
      }
      else 
        wifi->mqtt->publish("vehicles/blazer/aux-power/status", "off");
    }
  }
  
}

void Vehicle::report()
{
  reportDue = true;  
}

void Vehicle::checkSensors()
{

  // **
  // Process MCP IO - Read and Debounce Filter
  //
  for( int i = 0; i < NUM_IO; i++ ) {
    if (io[i].enabled && (io[i].direction == DIR_INPUT)) {
      // Debounce inputs
      // Increment the bounce count timer if we have
      // read in a state contrary to committed state
      // otherwise reset the counter
      bool val = (io[i].inverted ^ mcp->digitalRead(i));
      if (io[i].logic_state != val)
        io[i].bounce_count++;
      else io[i].bounce_count = 0;

      // We have passed the debounce filter check
      // Apply the IO state to registers
      if (io[i].bounce_count > 10) {
        io[i].logic_state = val;

        // Debugging output
        wifi->mqtt->writeLog(String(String("IO [") + String(i) + String("] change to:") + String(io[i].logic_state)).c_str(), L_DEBUG);
        Serial.print("IO [");
        Serial.print(i);
        Serial.print("] change: ");
        Serial.println(io[i].logic_state);
      }
    }
    // Default disabled inputs to logic state false
    if (!io[i].enabled && (io[i].direction == DIR_INPUT))
      io[i].logic_state = false;
  }

  switch (auxPowerMode) {

    // *** AUX_OFF ***
    case AUX_OFF:
    setAuxPower(false,false);
    break;

    // *** AUX_ON ***
    case AUX_ON:
    setAuxPower(true);
    break;

    // *** AUX_AUTO ***
    case AUX_AUTO:

    // Conditions for AUX power to be ON
    if (io[MCP_IN_ACC].logic_state ||
        io[MCP_IN_OEM_ALARM_DISARMING].logic_state ||
        io[MCP_IN_DOORS_LOCKING].logic_state ||
        io[MCP_IN_DOORS_UNLOCKING].logic_state ||
        io[MCP_IN_DOOR_PIN].logic_state)
    {
      // AUX Power Conditions for ON
      statusOutputPowerAuxConditions = true;
      // This could be occuring from AUX_OFF, or even if AUX power
      // is already in a countdown to turn off
      setAuxPower();
    } else {
      // Is this a TRANSITION to OFF?
      // Should probably instead track the timer
      if (statusOutputPowerAuxConditions) {

        // set up a timer so we can turn off later
        powerAuxOffTimeSoft = millis() + powerAuxOffTimeLength;

        // AUX Power Conditions for OFF
        statusOutputPowerAuxConditions = false;
      }
    }
  
    // if the input accessory is off, check
    // if we have a timer running against it
    if (!statusOutputPowerAuxConditions && statusOutputPowerAux && !powerAuxInSoftShutdown) {
        
        // Calculate time in seconds until Aux power will be turned off
        powerAuxTimeUntilOffSecs = (powerAuxOffTimeSoft - millis()) / 1000;
  
        // Time to begin the soft shutdown
        if (millis() >= powerAuxOffTimeSoft) {
          wifi->mqtt->publish("logs/esp/debug","hit powerAuxOffTimeSoft");
          Serial.println("hit powerAuxOffTimeSoft");
          setAuxPower(false);
        }
    } else powerAuxTimeUntilOffSecs = 0;
    
    break;
  }

  // Are we in a Power AUX SoftShutdown
  if (powerAuxInSoftShutdown) {
    if (millis() >= powerAuxOffTimeHard) {
      wifi->mqtt->publish("logs/esp/debug","hit powerAuxOffTimeHard");
      setAuxPower(false,false);
    }
  }

  // Request OneWire DS18B20 Temperatures
  // this->sensors->requestTemperatures();

  // Measure Supply Voltage
  this->voltageSupply = getVoltageSupply();
  // Serial.println(voltageSupply);
}

double Vehicle::getVoltageSupply(void)
{
	return ((double)analogRead(A0) * config->voltageDividerConstant) + config->voltageDividerCalibrateOffset;
}
