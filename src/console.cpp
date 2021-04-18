/*********************************************************************************
 * 
 * 
 * 
 ********************************************************************************/
#include <WiFiManager.h>
#include <RedGlobals.h>


dConsole console;

void handleConsole()
{
  // console
  if (console.check())
  {
    char str[128];

    if (strcmp(console.commandString,  "?") == 0)
    {
      console.println("\n\n\n[RED]HeatPump");
      console.print("IP address: ");
      console.println(WiFi.localIP().toString());
      console.println("Available commands are: location room, mqtt server port, setting, status, verbose, reset (Factory), reboot");
    }
    if  (strcmp(console.commandString, "reset") == 0)
    {
      console.print("Reseting configuration...");
      //reset settings - for testing
      WiFiManager wifiManager;
      wifiManager.resetSettings();
      console.println(" Done.");

    }
    if  (strcmp(console.commandString,  "reboot") == 0)
    {
      console.print("Rebooting...");
      delay(200);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }

    if (strcmp(console.commandString, "status") == 0)
      {
        heatpumpStatus hpStatus;
        hpStatus = hp.getStatus();
        sprintf(str, "Room Temp: %iF, operating %i, Freq %i", hp.CelsiusToFahrenheit(hpStatus.roomTemperature), hpStatus.operating, hpStatus.compressorFrequency);
        console.println(str);

        sprintf(str, "MQTT Server %s, port: %s", mqttServer, mqttPort);
        console.println(str);
      }
    if (strcmp(console.commandString,  "setting") == 0)
    {
      heatpumpSettings hps;
      hps = hp.getSettings();
      console.printf("Power Setting: %s\n",hps.power);
      console.printf("Mode Setting: %s\n", hps.mode);
      sprintf(str, "Set Temp: %iF", hp.CelsiusToFahrenheit(hps.temperature));
      console.println(str);

      if (!hps.connected) console.print("Not ");
      console.println("connected.");

      if (hpConnected) console.println("Initial connection ON");
      else console.println("Initial connection OFF");
    }

    if (getValue(console.commandString,' ',0) == "mqtt")
    {
      getValue(console.commandString,' ',1).toCharArray(mqttServer,64);
      console.println(mqttServer);
      getValue(console.commandString,' ',2).toCharArray(mqttPort,6);
      console.println(mqttPort);
      mqtt_client.setServer(mqttServer, int(mqttPort));
      writeConfigToDisk();
      console.print("MQTT server changed to ");
      console.println(mqttServer);
  
    }
if (getValue(console.commandString,' ',0) == "location")
{
    console.println(deviceLocation);
    console.println(myHostName);
    String newLocation = getValue(console.commandString,' ',1);
    if (newLocation.length() > 0)
    {
      newLocation.toCharArray(deviceLocation,64);
      writeConfigToDisk();
      console.print("Heatpump location changed to ");
      console.println(newLocation);
      console.println("Change will take effect after next reboot");
    }
}

    console.print("[RED]> ");
  }

}
/*
 * ********************************************************************************
 * 
 * ********************************************************************************
 */

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
