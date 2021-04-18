/************************************************************************************

   HeatPump ESP

   Configuration parameters are found in .h file


 *************************************************************************************/
#include <Arduino.h>
#include <RedGlobals.h>


// WiFiClient espClient;
// PubSubClient mqtt_client(espClient);
HeatPump hp;
bool hpConnected; // flag is we are connected to the HP
const bool isCelsius = false; // true = Celsius, false = Fahreneit

float remoteTemp;   // keeps room temperature as reported by remote thermostat
unsigned long lastTempSend;
unsigned long lastRemoteTemp; //holds last time a remote temp value has been received from OpenHAB

Ticker ticker;
/*
 * ********************************************************************************

 a few routines to drive the onboard blueLED

 * ********************************************************************************
*/
void ledON()
{
  digitalWrite(blueLED, false);
}
void ledOFF()
{
  digitalWrite(blueLED, true);
}

void tick()
{
  //toggle state
  int state = digitalRead(blueLED); // get the current state of GPIO1 pin
  digitalWrite(blueLED, !state);    // set pin to the opposite state
}

/*
 * ********************************************************************************

   Perform the initial hardware setup and then sequence the starting of the
   various modules

 * ********************************************************************************
*/

void setup() {

  pinMode(blueLED, OUTPUT);
  
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  console.enableSerial(&Serial, true);
  console.print("[RED]Heatpump ");
  console.println(VERSION);

  // Configure WIFI
  configureESP(); // load configuration from FLASH & configure WIFI

  digitalWrite(blueLED, LOW);
  console.enableTelnet(23);
  console.enableUDP(WiFi.localIP(), 10110);
  console.print("Connected! IP address: ");
  console.println(WiFi.localIP().toString());

  configureMQTT();

#ifdef _USE_WEB_SERVER
  configureWeb(); // configure the web UI
  console.println("Webserver started");
#endif


  // now configure the heat pump
  console.disableSerial();  // Stop using serial port as a console!

  // connect to the heatpump. Callbacks first so that the hpPacketDebug callback is available for connect()
  hp.setSettingsChangedCallback(hpSettingsChanged);
  hp.setStatusChangedCallback(hpStatusChanged);
  hp.setPacketCallback(hpPacketDebug);

  remoteTemp = 0;

  // configure the heat pump software
  hpConnected = hp.connect(&Serial);    // using Serial
  lastTempSend = millis();
  lastRemoteTemp = millis();

  if (hpConnected) 
    digitalWrite(blueLED, HIGH);
  else 
    digitalWrite(blueLED, LOW);

#ifdef _USE_THERMOSTAT
  configSensors(_TEMP_PERIOD, &updateTemperature);
#endif

}


/*
 * ********************************************************************************

   main loop services all modules: Wifi, MQTT, HP, console and webserver

 * ********************************************************************************
*/
void loop() {

  checkConnection();

  // handle Heat Pump
  hp.sync();

  //only send the temperature every 60s (default)
  if ((unsigned long)(millis() - lastTempSend) >= SEND_ROOM_TEMP_INTERVAL_MS) {
    hpStatusChanged(hp.getStatus());
    lastTempSend = millis();
    tick();

    // and send settings back while you are at it
    hpSettingsChanged();
    console.println("Temp sent");
  }

#ifdef _USE_WEB_SERVER
  // handle Web server requests
  handleWeb();
#endif

  checkMQTTConnection(); // check MQTT
  handleConsole(); // handle any commands from console

}


/*
 * ********************************************************************************

   This routine deals with updating the HP settings.

   It is assumed that currentSettings contains the parsed packet received from HP
   the values are extracted into json object and posted to MQTT

   This is called directly from HP when it received any changes
   or periodically to make sure MQTT server has good value (usefull during debug)

 * ********************************************************************************
*/

void hpSettingsChanged() {
  const size_t bufferSize = JSON_OBJECT_SIZE(6);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.createObject();

  heatpumpSettings currentSettings = hp.getSettings();

  root["power"]       = currentSettings.power;
  root["mode"]        = currentSettings.mode;
  root["temperature"] = (isCelsius) ? currentSettings.temperature : hp.CelsiusToFahrenheit(currentSettings.temperature);
  root["fan"]         = currentSettings.fan;
  root["vane"]        = currentSettings.vane;
  root["wideVane"]    = currentSettings.wideVane;

  char buffer[512];
  root.printTo(buffer, sizeof(buffer));

  if (!mqtt_client.publish(mqtt_topic, buffer, retain)) {
    mqtt_client.publish(mqtt_debug_topic, "failed to publish to heatpump topic");
  }
}
/*
 * ********************************************************************************

 * ********************************************************************************
*/

void hpStatusChanged(heatpumpStatus currentStatus) {
  // send room temp and operating info
  const size_t bufferSizeInfo = JSON_OBJECT_SIZE(2);
  DynamicJsonBuffer jsonBufferInfo(bufferSizeInfo);

  JsonObject& rootInfo = jsonBufferInfo.createObject();
  rootInfo["roomTemperature"] = (isCelsius) ? hp.getRoomTemperature() : hp.CelsiusToFahrenheit(hp.getRoomTemperature());
  rootInfo["operating"]       = currentStatus.operating;

  char bufferInfo[512];
  rootInfo.printTo(bufferInfo, sizeof(bufferInfo));

  if (!mqtt_client.publish(mqtt_status_topic, bufferInfo, true))
  {
    mqtt_client.publish(mqtt_debug_topic, "failed to publish to room temp and operation status to heatpump/status topic");
  }

  // send the timer info
  const size_t bufferSizeTimers = JSON_OBJECT_SIZE(5);
  DynamicJsonBuffer jsonBufferTimers(bufferSizeTimers);

  JsonObject& rootTimers = jsonBufferTimers.createObject();
  rootTimers["mode"]          = currentStatus.timers.mode;
  rootTimers["onMins"]        = currentStatus.timers.onMinutesSet;
  rootTimers["onRemainMins"]  = currentStatus.timers.onMinutesRemaining;
  rootTimers["offMins"]       = currentStatus.timers.offMinutesSet;
  rootTimers["offRemainMins"] = currentStatus.timers.offMinutesRemaining;

  char bufferTimers[512];
  rootTimers.printTo(bufferTimers, sizeof(bufferTimers));

  if (!mqtt_client.publish(mqtt_timers_topic, bufferTimers, true)) {
    mqtt_client.publish(mqtt_debug_topic, "failed to publish timer info to heatpump/status topic");
  }
}
/*
 * ********************************************************************************

 * ********************************************************************************
*/

void hpPacketDebug(byte * packet, unsigned int length, char* packetDirection) {
  if (_debugMode) {

    tick();   // toggle lights
    String message;
    for (unsigned int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0"; // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }

    const size_t bufferSize = JSON_OBJECT_SIZE(1);
    DynamicJsonBuffer jsonBuffer(bufferSize);

    JsonObject& root = jsonBuffer.createObject();

    root[packetDirection] = message;

    char buffer[512];
    root.printTo(buffer, sizeof(buffer));

    if (!mqtt_client.publish(mqtt_debug_topic, buffer))
    {
      mqtt_client.publish(mqtt_debug_topic, "failed to publish to heatpump/debug topic");
    }
  }
}
