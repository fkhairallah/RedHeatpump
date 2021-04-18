/************************************************************************************

   HeatPump ESP

   Configuration parameters are found in .h file


 *************************************************************************************/
//#include <Arduino.h>
//#include <RedGlobals.h>
#include "RedHeatPump.h"


WiFiClient espClient;
PubSubClient mqtt_client(espClient);
HeatPump hp;
bool hpConnected; // flag is we are connected to the HP
bool otaInProgress; // flags if OTA is in progress

float remoteTemp;   // keeps room temperature as reported by remote thermostat
unsigned long lastTempSend;
unsigned long lastRemoteTemp; //holds last time a remote temp value has been received from OpenHAB
unsigned long wifiMillis; //holds millis for counting up to hard reset for wifi reconnect
int secondsWithoutMQTT;

/*
 * ********************************************************************************

   Perform the initial hardware setup and then sequence the starting of the
   various modules

 * ********************************************************************************
*/

void setup() {

  pinMode(blueLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  console.enableSerial(&Serial, true);
  console.print("[RED]Heatpump ");
  console.println(version);

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


#ifdef _USE_THERMOSTAT
  // service temperature and other sensos
  serviceSensors();
#endif

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

  //reset to local temp sensor after 5 minutes of no remote temp udpates
  if ((unsigned long)(millis() - lastRemoteTemp) >= 300000) {
    hp.setRemoteTemperature(0);
    lastRemoteTemp = millis();
    console.println("No remote temp in 5 min. Resetting heatpump to use local thermostat");
  }

  // handle MQTT by reconnecting if necessary then executing a mqtt.loop() to service requests
  if (!mqtt_client.connected())
  {
    ticker.attach(1, tick); // start blink
    mqttConnect();  // retry connection
    delay(100);     // idle for a bit of time
    if (secondsWithoutMQTT++ > 600) // after a few minutes -- reset
    {
      console.print("Failed to connect to MQTT! What to do????");
      if (console.isTelnetConnected())
      {
        console.println(" Nothing. Telnet connected");
        secondsWithoutMQTT = 0;
      }
      else
      {
        console.println("Resetting...");
        delay(200);
        ESP.reset(); //reset and try again
        delay(5000);
      }
    }
  }
  else
  {
      // connected -- stop blinking and reset counter
      ticker.detach();
      secondsWithoutMQTT = 0;
  }

  mqtt_client.loop();

  // handle any commands from console
  handleConsole();

#ifdef _USE_WEB_SERVER
  // handle Web server requests
  handleWeb();
#endif



}

/*
 * ********************************************************************************

 * ********************************************************************************
*/
#ifdef _USE_THERMOSTAT
void updateTemperature(float temp)
{
  char str[128];
  console.println("Reporting temp reading of " + String(temp));

  sprintf(str, "%.1f", temp);
  mqtt_client.publish(heatpump_temperature_topic, str);

  tick();

}
#endif

/*
 * ********************************************************************************

   Configure the MQTT server by:
    - create all the topic using prefix/location/subtopic
    - configure MQTT server and port and setup callback routine
    - attempt a connection and log to debug topic if success

 * ********************************************************************************
*/

void configureMQTT()
{
  // configure the topics using location
  // heatpump/location/...
  sprintf(heatpump_topic, "%s/%s", mqttTopicPrefix, heatpumpLocation);
  sprintf(heatpump_set_topic, "%s/%s/set", mqttTopicPrefix, heatpumpLocation);
  sprintf(heatpump_status_topic, "%s/%s/status", mqttTopicPrefix, heatpumpLocation);
  sprintf(heatpump_timers_topic, "%s/%s/timers", mqttTopicPrefix, heatpumpLocation);
  sprintf(heatpump_temperature_topic, "%s/%s/temperature", mqttTopicPrefix, heatpumpLocation);
  sprintf(heatpump_debug_topic, "%s/%s/debug", mqttTopicPrefix, heatpumpLocation);
  sprintf(heatpump_debug_set_topic, "%s/%s/debug/set", mqttTopicPrefix, heatpumpLocation);

  // configure mqtt connection
  mqtt_client.setServer(mqttServer, atoi(mqttPort));
  mqtt_client.setCallback(mqttCallback);

  console.print("MQTT Server :'");
  console.print(mqttServer);
  console.print("' Port: ");
  console.print(String(atoi(mqttPort)));
  console.print(" Topic set to: '");
  console.print(heatpump_topic);
  console.println("'");


}

/*
 * ********************************************************************************

   attemps a connection to the MQTT server. if it fails increment secondsWithoutMQTT
   and return.
   This code relies on an existing Wifi connection which checked and dealt with
   elsewhere in the code

   Future code might turn the webserver on/off depending on MQTT connection

 * ********************************************************************************
*/

bool mqttConnect() {

  if (!mqtt_client.connected()) {
    // Attempt to connect
    if (mqtt_client.connect(myHostName, mqttUser, mqttPwd))
    {
      mqtt_client.subscribe(heatpump_set_topic);
      mqtt_client.subscribe(heatpump_debug_set_topic);
      console.println(heatpump_debug_set_topic);
      console.println("Connected to MQTT");
      char str[128];
      sprintf(str, "heatpump %s booted at %i.%i.%i.%i", heatpumpLocation, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
      mqtt_client.publish(heatpump_debug_topic, str, true);
      secondsWithoutMQTT = 0;
      return true;
    }
    else
    {
      secondsWithoutMQTT++;
      return false;
    }
  }
}


/*
 * ********************************************************************************

   This routine handles all MQTT callbacks and processes the commands sent to hp
   1. it changes the configuration sent to /set topic
   2. it updates the remote temp sent to /set
   3. it sends custom packets sent to /set (NOT USED)
   4. turns debug on/off

 * ********************************************************************************
*/

void mqttCallback(char* topic, byte * payload, unsigned int length) {

  // Copy payload into message buffer
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  //if the incoming message is on the heatpump_set_topic topic...
  if (strcmp(topic, heatpump_set_topic) == 0) {
    // Parse message into JSON
    const size_t bufferSize = JSON_OBJECT_SIZE(6);
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root = jsonBuffer.parseObject(message);

    if (!root.success()) {
      mqtt_client.publish(heatpump_debug_topic, "!root.success(): invalid JSON on heatpump_set_topic...");
      return;
    }

    // Step 3: Retrieve the values
    if (root.containsKey("power")) {
      String power = root["power"];
      hp.setPowerSetting(power);
    }

    if (root.containsKey("mode")) {
      String mode = root["mode"];
      hp.setModeSetting(mode);
    }

    if (root.containsKey("temperature")) {
      float temperature = (isCelsius) ? root["temperature"] : hp.FahrenheitToCelsius(root["temperature"]);
      hp.setTemperature( temperature );
    }

    if (root.containsKey("fan")) {
      String fan = root["fan"];
      hp.setFanSpeed(fan);
    }

    if (root.containsKey("vane")) {
      String vane = root["vane"];
      hp.setVaneSetting(vane);
    }

    if (root.containsKey("wideVane")) {
      String wideVane = root["wideVane"];
      hp.setWideVaneSetting(wideVane);
    }

    if (root.containsKey("remoteTemp")) {
      float remoteTemp = (isCelsius) ? root["remoteTemp"] : hp.FahrenheitToCelsius(root["remoteTemp"]);
      hp.setRemoteTemperature( remoteTemp );
      lastRemoteTemp = millis();
    }
    else if (root.containsKey("custom")) {
      String custom = root["custom"];

      // copy custom packet to char array
      char buffer[(custom.length() + 1)]; // +1 for the NULL at the end
      custom.toCharArray(buffer, (custom.length() + 1));

      byte bytes[20]; // max custom packet bytes is 20
      int byteCount = 0;
      char *nextByte;

      // loop over the byte string, breaking it up by spaces (or at the end of the line - \n)
      nextByte = strtok(buffer, " ");
      while (nextByte != NULL && byteCount < 20) {
        bytes[byteCount] = strtol(nextByte, NULL, 16); // convert from hex string
        nextByte = strtok(NULL, "   ");
        byteCount++;
      }

      // dump the packet so we can see what it is. handy because you can run the code without connecting the ESP to the heatpump, and test sending custom packets
      hpPacketDebug(bytes, byteCount, "customPacket");

      hp.sendCustomPacket(bytes, byteCount);
    }
    else {
      bool result = hp.update();

      if (!result) {
        mqtt_client.publish(heatpump_debug_topic, "heatpump: update() failed");
      }
    }
  }
  //if the incoming message is on the heatpump_debug_set_topic topic...
  else if (strcmp(topic, heatpump_debug_set_topic) == 0) {
    if (strcmp(message, "on") == 0) {
      _debugMode = true;
      mqtt_client.publish(heatpump_debug_topic, "debug mode enabled");
    } else if (strcmp(message, "off") == 0) {
      _debugMode = false;
      mqtt_client.publish(heatpump_debug_topic, "debug mode disabled");
    }
  } else {
    mqtt_client.publish(heatpump_debug_topic, strcat("heatpump: wrong mqtt topic: ", topic));
  }
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

  if (!mqtt_client.publish(heatpump_topic, buffer, retain)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish to heatpump topic");
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

  if (!mqtt_client.publish(heatpump_status_topic, bufferInfo, true)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish to room temp and operation status to heatpump/status topic");
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

  if (!mqtt_client.publish(heatpump_timers_topic, bufferTimers, true)) {
    mqtt_client.publish(heatpump_debug_topic, "failed to publish timer info to heatpump/status topic");
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
    for (int idx = 0; idx < length; idx++) {
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

    if (!mqtt_client.publish(heatpump_debug_topic, buffer)) {
      mqtt_client.publish(heatpump_debug_topic, "failed to publish to heatpump/debug topic");
    }
  }
}
