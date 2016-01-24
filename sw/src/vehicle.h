#ifndef VEHICLE_H
#define VEHICLE_H

#include "config.h"
#include "wifi.h"
#include "Arduino.h"
#include <Ticker.h>

// Maxim OneWire Interface
#include <OneWire.h>
#include <DallasTemperature.h>
#define MAX_DS1820_SENSORS 1

// ESP8266 ADC
#define VOLTAGE_BATTERY_ADC 0

// MCP23017 I2C IO Expander
#include <Adafruit_MCP23017.h>

#define NUM_IO 16

#define MCP_OUT_PWR_AUX 9
#define MCP_OUT_REQ_AUX_SHUTDOWN 10
// #define MCP_OUT_LOCK_DOORS 0
// #define MCP_OUT_ARM_ALARM 0

const uint8_t MCP_IN_ACC = 0;

#define MCP_IN_RESP_SBC_SHUTDOWN 11
// #define MCP_IN_DOME_LIGHTS 0

enum IoDirection { DIR_INPUT, DIR_OUTPUT };

struct Io {
  IoDirection direction;
  String description;
  bool logic_state;
  bool enabled;
  bool inverted;
  bool pullup;
};

class Vehicle 
{
	
	public:
		Vehicle(Config* config);
    void init();
    void loop();

    void setAuxPower(bool value = true, bool soft = true);
    
	private:

    Config* config;

    Wifi* wifi;

    double getVoltageSupply(void);
    void checkSensors();

    Io io[NUM_IO];

    // Vehicle Status
		double voltageSupply;
		bool statusInputAccessory;
    bool statusOutputPowerAux;
		
		double voltageDividerConstant;
		double voltageDividerCalibrateOffset;
    byte addr[MAX_DS1820_SENSORS][8];

    Adafruit_MCP23017* mcp;
    DallasTemperature* sensors;
    OneWire* oneWire;

    // Timer and State Management
    Ticker reportTimer;
    static bool reportDue;
    static void report();
    char buff[256]; // improve this

    // Off refers to warning begin -- we consider this to be OFF
    // The grace period after this point is intended to prevent filesystem
    // corruption on embedded systems only
    
    unsigned long powerAuxOffTimeLength; // amount of time aux power should remain on after accessorry off
    unsigned long powerAuxOffTimeLengthSoft; // time between soft warning and hard off
    
    unsigned long powerAuxOffTimeSoft; // actual time power aux should turn off
    unsigned long powerAuxOffTimeHard; // actual time power aux should turn off
    bool powerAuxInSoftShutdown;
    unsigned long powerAuxTimeUntilOffSecs;
    
    unsigned long inputAccessoryOffTime; // time accessory input went off
    
    unsigned long lastSensorCheck;
    
};

#endif
