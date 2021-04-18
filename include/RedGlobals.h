
#ifndef RED_GLOBALS_H
#define RED_GLOBALS_H
#include <dConsole.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <HeatPump.h>
#include <ArduinoJson.h>

#ifndef _PINS_H
#include <pins.h>
#endif

/*
 * ********************************************************************************
 *            START CONFIGURATION SECTION
 * ********************************************************************************
*/


#define VERSION "V1.2"
#define MQTT_TOPIC_PREFIX "heatpump" // prefix for all MQTT topics

const unsigned int SEND_ROOM_TEMP_INTERVAL_MS = 60000;

// in WiFiConfigurations.ino
extern char myHostName[];
extern char deviceLocation[];
extern char mqttServer[];
extern char mqttPort[];
extern char mqttUser[];
extern char mqttPwd[];
void configureESP();       // load configuration from FLASH & configure WIFI
void checkConnection(); // check WIFI connection
void writeConfigToDisk();
void configureOTA(char *hostName);

// in MQTT
extern PubSubClient mqtt_client;
extern char mqtt_topic[];
extern char mqtt_set_topic[];       //listens for commands
extern char mqtt_status_topic[]; //sends room temp and operation status
extern char mqtt_timers_topic[]; //timers
extern char mqtt_debug_topic[];
extern char mqtt_debug_set_topic[];
void configureMQTT();
bool checkMQTTConnection();
void mqttDisconnect();
void mqttCallback(char *topic, byte *payload, unsigned int length);
extern bool _debugMode;
extern bool retain; //change to false to disable mqtt retain

// in console.cpp
extern dConsole console;
void setupConsole();
void handleConsole();
String getValue(String data, char separator, int index);


// in RedHeatpump.cpp
extern HeatPump hp;
extern bool hpConnected; // flag is we are connected to the HP
extern const bool isCelsius;
extern unsigned long lastRemoteTemp;
void hpSettingsChanged();
void hpStatusChanged(heatpumpStatus currentStatus);
void hpPacketDebug(byte *packet, unsigned int length, char *packetDirection);

#endif
