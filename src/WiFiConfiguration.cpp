/*
 * ********************************************************************************

 This file manages the top-level WIFI and board configuration for [Red] based projects
 
 - Factory-fresh or unable to connect to WIFI it fires up a hotspot and a web server and allows the user to
    configure device parameters

 - If a WIFI configuration exists, it connects then reads the configuration from config.json
   and proceeds.

 * ********************************************************************************
*/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFi.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <LittleFS.h>

#include <RedGlobals.h>

// configuration parameters
// Hostname, AP name & MQTT clientID
char myHostName[64];

//flag for saving data
bool shouldSaveConfig = false;
int secondsWithoutWIFI;

bool otaInProgress; // flags if OTA is in progress

//for LED status
Ticker wticker;


/*
 * ********************************************************************************

  ********************  CUSTOMIZABLE SECTION  ***************************

 * ********************************************************************************
*/

//define your default values here, if there are different values in config.json, they are overwritten.
char deviceLocation[64] = "NEW";
char mqttServer[64] = "RyeManorPi.local";
char mqttPort[16] = "1883";
char mqttUser[64] = "";
char mqttPwd[64] = "";

// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
WiFiManagerParameter custom_deviceLocation("location", "Device Location", deviceLocation, 64);
WiFiManagerParameter custom_mqttServer("server", "mqtt server", mqttServer, 64);
WiFiManagerParameter custom_mqttPort("port", "mqtt port", mqttPort, 16);
WiFiManagerParameter custom_mqttUser("user", "mqtt user", mqttUser, 64);
WiFiManagerParameter custom_mqttPwd("pwd", "mqtt password", mqttPwd, 64);

// load parameters form JSON that was saved to disk
void loadParametersfromJSON(DynamicJsonDocument json)
{
  /* common */
  if (json.containsKey("deviceLocation"))  strcpy(deviceLocation, json["deviceLocation"]);
  if (json.containsKey("mqttServer"))      strcpy(mqttServer, json["mqttServer"]);
  if (json.containsKey("mqttPort"))        strcpy(mqttPort, json["mqttPort"]);
  if (json.containsKey("mqttUser"))        strcpy(mqttUser, json["mqttUser"]);
  if (json.containsKey("mqttPwd"))        strcpy(mqttPwd, json["mqttPwd"]);

}

// save parameters to a JSON object so they can saved to disk
DynamicJsonDocument saveParametersToJSON()
{
  //JsonObject &json = jsonBuffer.createObject();
  DynamicJsonDocument json(200);
  /* common */
  json["deviceLocation"] = deviceLocation;
  json["mqttServer"] = mqttServer;
  json["mqttPort"] = mqttPort;
  json["mqttUser"] = mqttUser;
  json["mqttPwd"] = mqttPwd;

  return json;
}

// load parameters into webserver custom data slots
void loadParametersToWeb(WiFiManager* wfm)
{
  /* common */
  wfm->addParameter(&custom_deviceLocation);
  wfm->addParameter(&custom_mqttServer);
  wfm->addParameter(&custom_mqttPort);
  wfm->addParameter(&custom_mqttUser);
  wfm->addParameter(&custom_mqttPwd);

  /* custom */
}

// this is called by the SUBMIT action of the webserver
// it copies the server returned data into parameter locations
void saveParametersFromWeb()
{
    //copy updated parameters into proper location
    strcpy(deviceLocation, custom_deviceLocation.getValue());
    strcpy(mqttServer, custom_mqttServer.getValue());
    strcpy(mqttPort, custom_mqttPort.getValue());
    strcpy(mqttUser, custom_mqttUser.getValue());
    strcpy(mqttPwd, custom_mqttPwd.getValue());

    console.println("Should save config");
    shouldSaveConfig = true;
  
}


/*
 * ********************************************************************************

    ********************  END OF CUSTOMIZABLE SECTION  ***************************

 * ********************************************************************************
*/

/*
 * ********************************************************************************

 simple routine to toggle LED indicator

 * ********************************************************************************
*/

void wtick()
{
  //toggle state
  int state = digitalRead(blueLED);  // get the current state of GPIO1 pin
  digitalWrite(blueLED, !state);     // set pin to the opposite state
}

void tickOFF()
{
  digitalWrite(blueLED,HIGH);
}



/*
 * ********************************************************************************
 * This routine will check the Wifi status, and reset the ESP is unable to connect
 * 
 * If all is well it initiates mDNS service & OTA process
 * 
 * ********************************************************************************
*/

void checkConnection()
{
  if (WiFi.status() != WL_CONNECTED) //reconnect wifi
  {
    console.println("Not connected to WIFI.. give it ~10 seconds.");
    delay(1000);
    if (secondsWithoutWIFI++ > 30)
    {
      ESP.reset();
      delay(5000);
    }
  }
  
  MDNS.update();      // and refresh mDNS

  // handle OTA -- if in progress stop talking to the heat pump and console so as not to disturb the upload
  // THIS NEEDS TO BE THE FIRST ITEM IN LOOP
  ArduinoOTA.handle();

}


/*
 * ********************************************************************************

 read device configuration from config.json in the file system

 * ********************************************************************************
*/
void readConfigFromDisk()
{
  //read configuration from FS json
  //console.println("mounting FS...");

  if (LittleFS.begin())
  {
    console.println("mounted file system");
    if (LittleFS.exists("/config.json"))
    {
      //file exists, reading and loading
      //console.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        //console.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(200);
        auto error = deserializeJson(json, buf.get());
        if (!error)
        {
          loadParametersfromJSON(json);
        }
        else 
        {
          console.println("failed to load json config");
        }
        configFile.close();

      }
    }
  }
  else
  {
    console.println("failed to mount FS");
  }
  //end read

}
/*
 * ********************************************************************************

 updates the configuration in config.json in the file system

 * ********************************************************************************
*/
void writeConfigToDisk()
{


  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    console.println("failed to open config file for writing");
  }
  else
  {
    DynamicJsonDocument json = saveParametersToJSON();
    serializeJson(json,configFile);
    configFile.close();
    console.println("config saved to disk");
  }
}


/*
 * ********************************************************************************

 * ********************************************************************************
*/

void configureESP()
{

  //clean FS, for testing
  //SPIFFS.format();

  readConfigFromDisk();


  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // don't output shit in Serial port -- it messes with heatpump
  wifiManager.setDebugOutput(false);


  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback([](WiFiManager * myWIFI) {
    console.println("Entered config mode");
    console.println(WiFi.localIP().toString());
    //if you used auto generated SSID, print it
    console.println(myWIFI->getConfigPortalSSID());
    //entered config mode, make led toggle faster
    wticker.attach(0.2, wtick);
  });

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveParametersFromWeb);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  loadParametersToWeb(&wifiManager);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout (in seconds) until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  wifiManager.setTimeout(120);

  sprintf(myHostName, "%s-%s", MQTT_TOPIC_PREFIX, deviceLocation);
  console.print("Hostname: ");
  console.println(myHostName);

  WiFi.hostname(myHostName);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(myHostName)) {
    console.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  console.println("Connected to: " + WiFi.SSID());

  //if you get here you have connected to the WiFi
  //console.println("connected...yeey :)");
  secondsWithoutWIFI = 0;
  wticker.detach();
  tickOFF();
  

  // configure mDNS so we can reach it via .local (sometimes)
  if (!MDNS.begin(myHostName)) {
    console.println("Error setting up MDNS responder!");
  }
  else
  {
    MDNS.addService("telnet", "tcp", 23); // Announce telnet tcp service on port 8080
    //MDNS.addServiceTxt(hMDNSService, "app_name", "CabinetsLED");
    //MDNS.addServiceTxt(hMDNSService, "app_version", version;    
    MDNS.update();

    console.println("mDNS responder started");
  }

  // and OTA
  configureOTA(myHostName);


  //if the portal changed the parameters then save the custom parameters to FS
  if (shouldSaveConfig)  writeConfigToDisk();


}

/*
 * ********************************************************************************
 * 
 * ********************************************************************************
 */
void configureOTA(char *hostName)
{
  // configure OTA
  otaInProgress = false; // used to stop other activities during OTA update

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostName);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else
    { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    console.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    otaInProgress = false;
    console.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char buffer[100];
    sprintf(buffer, "Progress: %u%%\r", (progress / (total / 100)));
    console.println(buffer);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    otaInProgress = false;
    char buffer[100];
    sprintf(buffer, "Error[%u]: ", error);
    console.println(buffer);
    if (error == OTA_AUTH_ERROR)
    {
      console.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      console.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      console.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      console.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      console.println("End Failed");
    }
  });

  ArduinoOTA.begin();
}
