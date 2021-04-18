
/**********************************************************************************
 *
 * Configure the MQTT server by:
 *     - create all the topic using prefix/location/subtopic
 *     - configure MQTT server and port and setup callback routine
 *     - attempt a connection and log to debug topic if success
 * 
 *********************************************************************************/
#include <RedGlobals.h>

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// mqtt client settings
char mqtt_topic[64] = "heatpump/default";                                           //contains current settings
char mqtt_set_topic[64] = "heatpump/default/set";                                   //listens for commands
char mqtt_status_topic[64] = "heatpump/default/status";                             //sends room temp and operation status
char mqtt_timers_topic[64] = "heatpump/default/timers";                             //timers

char mqtt_debug_topic[64] = "heatpump/default/debug";                               //debug messages
char mqtt_debug_set_topic[64] = "heatpump/default/debug/set";                       //enable/disable debug messages

int secondsWithoutMQTT;

// MQTT Settings
// debug mode, when true, will send all packets received from the heatpump to topic mqtt_debug_topic
// this can also be set by sending "on" to mqtt_debug_set_topic
bool _debugMode = false;
bool retain = true; //change to false to disable mqtt retain

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
  sprintf(mqtt_topic, "%s/%s", MQTT_TOPIC_PREFIX, deviceLocation);
  sprintf(mqtt_set_topic, "%s/%s/set", MQTT_TOPIC_PREFIX, deviceLocation);
  sprintf(mqtt_status_topic, "%s/%s/status", MQTT_TOPIC_PREFIX, deviceLocation);
  sprintf(mqtt_timers_topic, "%s/%s/timers", MQTT_TOPIC_PREFIX, deviceLocation);

  sprintf(mqtt_debug_topic, "%s/%s/debug", MQTT_TOPIC_PREFIX, deviceLocation);
  sprintf(mqtt_debug_set_topic, "%s/%s/debug/set", MQTT_TOPIC_PREFIX, deviceLocation);

  // configure mqtt connection
  mqtt_client.setServer(mqttServer, atoi(mqttPort));
  mqtt_client.setCallback(mqttCallback);

  console.print("MQTT Server :'");
  console.print(mqttServer);
  console.print("' Port: ");
  console.print(String(atoi(mqttPort)));
  console.print(" Topic set to: '");
  console.print(mqtt_topic);
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

bool checkMQTTConnection() {

  if (mqtt_client.connected()) 
  {
     // loop through the client
     mqtt_client.loop();
  }
  else 
  {
    // set server name
    mqtt_client.setServer(mqttServer, atoi(mqttPort) );

    // Attempt to connect
    if (mqtt_client.connect(myHostName))
    {
#ifdef DISPLAY_PRESENT
      mqtt_client.subscribe(mqtt_requiredTemperature_topic);
#endif
      mqtt_client.subscribe(mqtt_debug_set_topic);
      console.println("Connected to MQTT");
      char str[128];
      sprintf(str, "%s %s [%s] MQTT{%s,%s}  IP:%i.%i.%i.%i", MQTT_TOPIC_PREFIX, VERSION, deviceLocation, mqttServer, mqttPort, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
      mqtt_client.publish(mqtt_debug_topic, str, true);
      secondsWithoutMQTT = 0;
      return true;
    }
    else
    {
      delay(500);
      secondsWithoutMQTT++;
      return false;
    }
  }
  return true;
}

void mqttDisconnect()
{
  mqtt_client.disconnect();
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
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // Copy payload into message buffer
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++)
  {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  //if the incoming message is on the heatpump_set_topic topic...
  if (strcmp(topic, mqtt_set_topic) == 0)
  {
    // V5 - Parse message into JSON
    // const size_t bufferSize = JSON_OBJECT_SIZE(6);
    // DynamicJsonBuffer jsonBuffer(bufferSize);
    // JsonObject &root = jsonBuffer.parseObject(message);
    // if (!root.success())
    // {
    //   mqtt_client.publish(mqtt_debug_topic, "!root.success(): invalid JSON on heatpump_set_topic...");
    //   return;
    // }

    // V6 - Parse message into JSON
    DynamicJsonDocument root(200);
    auto error = deserializeJson(root, message);
    if (error) {
        mqtt_client.publish(mqtt_debug_topic, "!root.success(): invalid JSON on heatpump_set_topic...");
        return;
    }

    // Step 3: Retrieve the values
    if (root.containsKey("power"))
    {
      hp.setPowerSetting(strcmp(root["power"],"ON")?1:0);
    }

    if (root.containsKey("mode"))
    {
      hp.setModeSetting(root["mode"]);
    }

    if (root.containsKey("temperature"))
    {
      float temperature = (isCelsius) ? root["temperature"] : hp.FahrenheitToCelsius(root["temperature"]);
      hp.setTemperature(temperature);
    }

    if (root.containsKey("fan"))
    {
      hp.setFanSpeed(root["fan"]);
    }

    if (root.containsKey("vane"))
    {
      hp.setVaneSetting(root["vane"]);
    }

    if (root.containsKey("wideVane"))
    {
      hp.setWideVaneSetting(root["wideVane"]);
    }

    if (root.containsKey("remoteTemp"))
    {
      float remoteTemp = (isCelsius) ? root["remoteTemp"] : hp.FahrenheitToCelsius(root["remoteTemp"]);
      hp.setRemoteTemperature(remoteTemp);
      lastRemoteTemp = millis();
    }
    else if (root.containsKey("custom"))
    {
      String custom = root["custom"];

      // copy custom packet to char array
      char buffer[(custom.length() + 1)]; // +1 for the NULL at the end
      custom.toCharArray(buffer, (custom.length() + 1));

      byte bytes[20]; // max custom packet bytes is 20
      int byteCount = 0;
      char *nextByte;

      // loop over the byte string, breaking it up by spaces (or at the end of the line - \n)
      nextByte = strtok(buffer, " ");
      while (nextByte != NULL && byteCount < 20)
      {
        bytes[byteCount] = strtol(nextByte, NULL, 16); // convert from hex string
        nextByte = strtok(NULL, "   ");
        byteCount++;
      }

      // dump the packet so we can see what it is. handy because you can run the code without connecting the ESP to the heatpump, and test sending custom packets
      hpPacketDebug(bytes, byteCount, (char*)"customPacket");

      hp.sendCustomPacket(bytes, byteCount);
    }
    else
    {
      bool result = hp.update();

      if (!result)
      {
        mqtt_client.publish(mqtt_debug_topic, (char*) "heatpump: update() failed");
      }
    } 
  }
  //if the incoming message is on the heatpump_debug_set_topic topic...
  else if (strcmp(topic, mqtt_debug_set_topic) == 0)
  {
    if (strcmp(message, "on") == 0)
    {
      _debugMode = true;
      mqtt_client.publish(mqtt_debug_topic, (char *)"debug mode enabled");
    }
    else if (strcmp(message, "off") == 0)
    {
      _debugMode = false;
      mqtt_client.publish(mqtt_debug_topic, (char *)"debug mode disabled");
    }
  }
  else
  {
    mqtt_client.publish(mqtt_debug_topic, strcat((char *)"heatpump: wrong mqtt topic: ", topic));
  } 
}