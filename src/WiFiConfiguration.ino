/*
 * ********************************************************************************

 * ********************************************************************************
*/
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <DNSServer.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson


//flag for saving data
bool shouldSaveConfig = false;
int secondsWithoutWIFI;


// hold configurations as stored on disk
DynamicJsonBuffer jsonBuffer;

// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
WiFiManagerParameter custom_heatpumpLocation("location", "Heatpump Location", heatpumpLocation, 64);
WiFiManagerParameter custom_mqttServer("server", "mqtt server", mqttServer, 64);
WiFiManagerParameter custom_mqttPort("port", "mqtt port", mqttPort, 16);
WiFiManagerParameter custom_mqttUser("user", "mqtt user", mqttUser, 64);
WiFiManagerParameter custom_mqttPwd("pwd", "mqtt password", mqttPwd, 64);

/*
 * ********************************************************************************

 * ********************************************************************************
*/


void tick()
{
  //toggle state
  int state = digitalRead(blueLED);  // get the current state of GPIO1 pin
  digitalWrite(blueLED, !state);     // set pin to the opposite state
  digitalWrite(redLED, state);
}


/*
 * ********************************************************************************
 * This routine will check the Wifi status, and reset the ESP is unable to connect
 * 
 * If all is well is services mDNS & OTA process
 * 
 * ********************************************************************************
*/

void checkConnection()
{
  if (WiFi.status() != WL_CONNECTED) //reconnect wifi
  {
    console.println("Not connected to WIFI.. give it ~10 seconds.");
    delay(1000);
    if (secondsWithoutWIFI++ > 10)
    {
      ESP.reset();
      delay(5000);
    }
  }
  
  MDNS.update();      // and refresh mDNS

  // handle OTA -- if in progress stop talking to the heat pump and console so as not to disturb the upload
  // THIS NEEDS TO BE THE FIRST ITEM IN LOOP
  ArduinoOTA.handle();
  if (otaInProgress) return;

}


/*
 * ********************************************************************************

 * ********************************************************************************
*/


void readConfigFromDisk()
{
  //read configuration from FS json
  //console.println("mounting FS...");

  if (SPIFFS.begin()) {
    console.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      //console.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        //console.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        //json.printTo(Serial);
        //console.println();
        if (json.success()) {
          //console.println("\nparsed json");
          if (json["heatpumpLocation"].success()) strcpy(heatpumpLocation, json["heatpumpLocation"]);
          if (json["mqttServer"].success()) strcpy(mqttServer, json["mqttServer"]);
          if (json["mqttPort"].success()) strcpy(mqttPort, json["mqttPort"]);
          if (json["mqttUser"].success()) strcpy(mqttUser, json["mqttUser"]);
          if (json["mqttPwd"].success()) strcpy(mqttPwd, json["mqttPwd"]);

        } else {
          console.println("failed to load json config");
        }
        configFile.close();

      }
    }
  } else {
    console.println("failed to mount FS");
  }
  //end read

}
/*
 * ********************************************************************************

 * ********************************************************************************
*/
void writeConfigToDisk()
{
  console.println("saving config");

  JsonObject& json = jsonBuffer.createObject();
  json["heatpumpLocation"] = heatpumpLocation;
  json["mqttServer"] = mqttServer;
  json["mqttPort"] = mqttPort;
  json["mqttUser"] = mqttUser;
  json["mqttPwd"] = mqttPwd;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    console.println("failed to open config file for writing");
  }

  //json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save

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
    console.println(WiFi.softAPIP().toString());
    //if you used auto generated SSID, print it
    console.println(myWIFI->getConfigPortalSSID());
    //entered config mode, make led toggle faster
    ticker.attach(0.2, tick);
  });

  //set config save notify callback
  wifiManager.setSaveConfigCallback([] {
    //copy updated parameters into proper location
    strcpy(heatpumpLocation, custom_heatpumpLocation.getValue());
    strcpy(mqttServer, custom_mqttServer.getValue());
    strcpy(mqttPort, custom_mqttPort.getValue());
    strcpy(mqttUser, custom_mqttUser.getValue());
    strcpy(mqttPwd, custom_mqttPwd.getValue());

    console.println("Should save config");
    shouldSaveConfig = true;
  });

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_heatpumpLocation);
  wifiManager.addParameter(&custom_mqttServer);
  wifiManager.addParameter(&custom_mqttPort);
  wifiManager.addParameter(&custom_mqttUser);
  wifiManager.addParameter(&custom_mqttPwd);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout (in seconds) until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  wifiManager.setTimeout(120);

  sprintf(myHostName, "heatpump-%s", heatpumpLocation);
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



  //if you get here you have connected to the WiFi
  //console.println("connected...yeey :)");
  secondsWithoutWIFI = 0;
  ticker.detach();

  // configure mDNS so we can reach it via .local (sometimes)
  if (!MDNS.begin(myHostName)) {
    console.println("Error setting up MDNS responder!");
  }
  else
  {
    MDNS.addService("web", "tcp", 80); // Announce web tcp service on port 8080
    MDNS.addService("telnet", "tcp", 23); // Announce telnet tcp service on port 8080
    console.println("mDNS responder started");
  }

  // and OTA
  configureOTA(myHostName);


  //if the portal changed the parameters then save the custom parameters to FS
  if (shouldSaveConfig)  writeConfigToDisk();


}
