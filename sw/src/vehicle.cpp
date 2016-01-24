#include "vehicle.h"

// disabled DS18B20 out of fear that on reboot,
// ESP will enter bootloader mode if anything
// is communicating on the one-wire!
// check battery voltage every 1 second

Vehicle::Vehicle(Config* config_p)
{
  
  // Pass configuration in
  config = config_p;
  
  wifi = new Wifi(config);

  this->oneWire = new OneWire(2);
  this->sensors = new DallasTemperature(oneWire);
  this->mcp = new Adafruit_MCP23017();

  lastSensorCheck = 0;

  // Time AUX power will remain on after needed in ms
  powerAuxOffTimeLength = 20UL * 60UL * 1000UL; // 30 minutes
  powerAuxOffTimeLengthSoft = 2UL * 60UL * 1000UL; // 2 minutes
  powerAuxInSoftShutdown = false;

  // assume for the time being that the accessory input
  // is currently off
  statusInputAccessory = false;

  // Ti+cker Timers
  reportTimer.attach(5.0, report);

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
  }

  io[MCP_IN_ACC].enabled = true;
  
  io[MCP_OUT_PWR_AUX].direction = DIR_OUTPUT;
  io[MCP_OUT_PWR_AUX].enabled = true;

  io[MCP_OUT_REQ_AUX_SHUTDOWN].direction = DIR_OUTPUT;
  io[MCP_OUT_REQ_AUX_SHUTDOWN].enabled = true;

  io[MCP_IN_RESP_SBC_SHUTDOWN].enabled = true;

  // implement configuration with calls
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
    statusOutputPowerAux = false;
    if (soft) {
      mcp->digitalWrite(MCP_OUT_REQ_AUX_SHUTDOWN, HIGH);
      powerAuxOffTimeHard = millis() + powerAuxOffTimeLengthSoft;
      powerAuxInSoftShutdown = true;
    }
    else {
      mcp->digitalWrite(MCP_OUT_REQ_AUX_SHUTDOWN, HIGH);
      mcp->digitalWrite(MCP_OUT_PWR_AUX, LOW);
      powerAuxInSoftShutdown = false;
    }
  }
}

bool Vehicle::reportDue = false;

void Vehicle::loop()
{

  wifi->loop();

  // perform block once per second
  // lastSensorCheck is set to 0 in constructor
  if ((millis() - lastSensorCheck) > 1000) {
    checkSensors();
    lastSensorCheck = millis();
  }

  if (reportDue) {
    if ((WiFi.status() == WL_CONNECTED) && wifi->mqttc->pubsubc->connected()) {

      String payload;

      // IO Status
      payload = "{ ";
      for ( int i = 0; i < NUM_IO; i++ ) {
        payload += String(i);
        payload += " : ";
        if (io[i].logic_state) payload += "1";
        else payload += "0";
        if (i < (NUM_IO - 1)) payload += " , ";
      }
      payload += " }";
      payload.toCharArray(buff, 256);
      wifi->mqttc->pubsubc->publish("vehicles/blazer/io", buff);
      
      // Voltage, Supply (usually battery)
      payload = String(voltageSupply, 1);
      payload.toCharArray(buff, 10);
      wifi->mqttc->pubsubc->publish("vehicles/blazer/voltage", buff);

      // Temperature, Onboard
//      payload = String(sensors->getTempCByIndex(0), 1);
//      payload.toCharArray(buff, 10);
//      wifi->mqttc->pubsubc->publish("vehicles/blazer/temperature", buff);
      
      // Acc Input Status
      if (io[MCP_IN_ACC].enabled) {
        if (io[MCP_IN_ACC].logic_state)
          wifi->mqttc->pubsubc->publish("vehicles/blazer/input-acc/status", "low");
        else
          wifi->mqttc->pubsubc->publish("vehicles/blazer/input-acc/status", "high");
      }

      // Power Status, AUX
      if (statusOutputPowerAux) {
        wifi->mqttc->pubsubc->publish("vehicles/blazer/aux-power/status", "on");

        // Power Status, AUX - Time Remaining
        payload =  String(powerAuxTimeUntilOffSecs);
        payload.toCharArray(buff, 10);
        wifi->mqttc->pubsubc->publish("vehicles/blazer/aux-power/time-remaining", buff);
      }
      else 
        wifi->mqttc->pubsubc->publish("vehicles/blazer/aux-power/status", "off");
      
      reportDue = false;
    }
  }
  
}

void Vehicle::report()
{
  reportDue = true;  
}

void Vehicle::checkSensors()
{

  // Check MCP IO
  for( int i = 0; i < NUM_IO; i++ ) {
    if (io[i].enabled && (io[i].direction == DIR_INPUT)) {
      io[i].logic_state = mcp->digitalRead(i);
    }
  }

  // inverted
  bool state = !mcp->digitalRead(MCP_IN_ACC);
  if (statusInputAccessory != state) {
    statusInputAccessory = state;
    Serial.print("\r\nAccessory Status: ");
    Serial.println(state);
    if (state) {
      // This could be occuring from AUX_OFF, or even if AUX power
      // is already in a countdown to turn off
      setAuxPower();
    } else {
      // set up a timer so we can turn off later
      powerAuxOffTimeSoft = millis() + powerAuxOffTimeLength;
    }
  }

//  if (mqttc->pubsubc->connected())
//    this->mqttc->pubsubc->publish("logs/esp","booted");

  // if the input accessory is off, check
  // if we have a timer running against it
  if (!statusInputAccessory && statusOutputPowerAux) {
      
      // Calculate time in seconds until Aux power will be turned off
      powerAuxTimeUntilOffSecs = (powerAuxOffTimeSoft - millis()) / 1000;
      
      if (millis() > powerAuxOffTimeSoft) {
        setAuxPower(false);
      }
  } else powerAuxTimeUntilOffSecs = 0;

  if (powerAuxInSoftShutdown) {
    if (millis() > powerAuxOffTimeHard)
      setAuxPower(false,false);
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
