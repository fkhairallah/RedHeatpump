/*
 * ********************************************************************************
 *  v1.03: added mdns.update() to the main loop, 
 * 
 * ********************************************************************************
 */
const char *version = "version 1.03";

#include <ESP8266WiFi.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <dConsole.h>
#include <HeatPump.h>
#include <ArduinoJson.h>
#include <Ticker.h>

/*
 * ********************************************************************************
 *            START CONFIGURATION SECTION
 * ********************************************************************************
*/

// switch to define whether the web US is used or not
#define _USE_WEB_SERVER

// comment out to remove thermostat code
//#define _USE_THERMOSTAT
#define _TEMP_PERIOD 20000  // in ms the frequency of temperature reporting

// MQTT Settings
// debug mode, when true, will send all packets received from the heatpump to topic heatpump_debug_topic
// this can also be set by sending "on" to heatpump_debug_set_topic
bool _debugMode = false;
bool retain = true; //change to false to disable mqtt retain

// sketch settings
const unsigned int SEND_ROOM_TEMP_INTERVAL_MS = 60000;
const bool isCelsius = false;   // true = Celsius, false = Fahreneit

// prefix for all MQTT topics
const char* mqttTopicPrefix = "heatpump";

/*
 * ********************************************************************************
 *            END CONFIGURATION SECTION
 * ********************************************************************************
*/

#define redLED 0    // Red LED light on Huzzah
#define blueLED 2   // blue LED light on ESP12e
//for LED status
Ticker ticker;

dConsole console;

// configuration parameters
// Hostname, AP name & MQTT clientID
char myHostName[64];

//define your default values here, if there are different values in config.json, they are overwritten.
char heatpumpLocation[64];
char mqttServer[64];
char mqttPort[16] = "1883";
char mqttUser[64] = "";
char mqttPwd[64] = "";

// mqtt client settings

char heatpump_topic[64]              = "heatpump/default";  //contains current settings
char heatpump_set_topic[64]          = "heatpump/default/set"; //listens for commands
char heatpump_status_topic[64]       = "heatpump/default/status"; //sends room temp and operation status
char heatpump_timers_topic[64]       = "heatpump/default/timers"; //timers
char heatpump_temperature_topic[64]  = "thermostat/default/temperature"; //temperature

char heatpump_debug_topic[64]        = "heatpump/default/debug"; //debug messages
char heatpump_debug_set_topic[64]    = "heatpump/default/debug/set"; //enable/disable debug messages


void configModeCallback (WiFiManager *myWIFI);
