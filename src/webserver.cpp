/*
 * ********************************************************************************

 * ********************************************************************************
*/
#ifdef _USE_WEB_SERVER

ESP8266WebServer server(80);

const char* html = "<html>\n<head>\n<meta name='viewport' content='width=device-width, initial-scale=2'/>\n"
                   "<meta http-equiv='refresh' content='_RATE_; url=/'/>\n"
                   "<style></style>\n<body>"
                   "<h3>_LOCATION_ Heat Pump _VERSION_</h3>TEMP (Room/Set): _ROOMTEMP_\n&deg;F<form>\n<table>\n"
                   "<tr>\n<td>Power:</td>\n<td>\n_POWER_</td>\n</tr>\n"
                   "<tr>\n<td>Mode:</td>\n<td>\n_MODE_</td>\n</tr>\n"
                   "<tr>\n<td>Temp:</td>\n<td>\n_TEMP_</td>\n</tr>"
                   "<tr>\n<td>Fan:</td>\n<td>\n_FAN_</td>\n</tr>\n"
                   "<tr>\n<td>Vane:</td><td>\n_VANE_</td>\n</tr>\n"
                   "<tr>\n<td>WideVane:</td>\n<td>\n_WVANE_</td>\n</tr>\n"
                   "</table>\n<br/><input type='submit' value='Change Settings'/>\n</form><br/><br/>"
                   "<hr/>"
                   "<form><input type='submit' name='RoomTemp' value='Room Temp'/>\n"
                   "Room temp as reported by remote Thermostat: _REMOTETEMP_</form>\n"
                   "<hr/>"
                   "<h3>MQTT Server configuration</h3>"
                   "<form>\n<table>\n"
                   "<tr>\n<td>Server Name/IP:</td>\n<td>\n<input type='text' name='mqtt_server' value='_MQTT_SERVER_'></td>\n</tr>\n"
                   "<tr>\n<td>Port:</td>\n<td>\n<input type='text' name='mqtt_port' value='_MQTT_PORT_'></td>\n</tr>\n"
                   "<tr>\n<td>Username:</td>\n<td>\n<input type='text' name='mqtt_user' value='_MQTT_USER_'></td>\n</tr>\n"
                   "<tr>\n<td>Password:</td>\n<td>\n<input type='text' name='mqtt_pwd' value='_MQTT_PWD_'></td>\n</tr>\n"
                   "</table>\n<br/><input type='submit' name='mqtt_form' value='MQTT configuration'/>\n"
                   "<hr/>"
                   "<form><input type='submit' name='CONNECT' value='Re-Connect'/>\n</form>\n"
                   "</body>\n</html>\n";

/*
 * ********************************************************************************

 * ********************************************************************************
*/

void configureWeb()
{
  // Start the webserver
  server.on("/", handle_root);
  server.on("/generate_204", handle_root);
  server.onNotFound(handleNotFound);
  server.begin();
}

void pauseWeb()
{
  server.stop();
  
}

void resumeWeb()
{
  server.begin();
  
}
/*
 * ********************************************************************************

 * ********************************************************************************
*/

void handleWeb()
{
  server.handleClient();
}

/*
 * ********************************************************************************

 * ********************************************************************************
*/

void handle_root() {
  int rate = change_states() ? 0 : 60;
  String toSend = html;
  toSend.replace("_LOCATION_",heatpumpLocation);
  toSend.replace("_VERSION_", version);
  toSend.replace("_RATE_", String(rate));
  String power[2] = {"OFF", "ON"};
  toSend.replace("_POWER_", createOptionSelector("POWER", power, 2, hp.getPowerSetting()));
  String mode[5] = {"HEAT", "DRY", "COOL", "FAN", "AUTO"};
  toSend.replace("_MODE_", createOptionSelector("MODE", mode, 5, hp.getModeSetting()));
  String temp[16] = {"31", "30", "29", "28", "27", "26", "25", "24", "23", "22", "21", "20", "19", "18", "17", "16"};
  String tempF[31] = {"HP", "61", "62", "63", "64", "65", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "90"};
  toSend.replace("_TEMP_", createOptionSelector("TEMP", tempF, 31, String(hp.CelsiusToFahrenheit(hp.getTemperature()))));
  String fan[6] = {"AUTO", "QUIET", "1", "2", "3", "4"};
  toSend.replace("_FAN_", createOptionSelector("FAN", fan, 6, hp.getFanSpeed()));
  String vane[7] = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
  toSend.replace("_VANE_", createOptionSelector("VANE", vane, 7, hp.getVaneSetting()));
  String widevane[7] = {"<<", "<", "|", ">", ">>", "<>", "SWING"};
  toSend.replace("_WVANE_", createOptionSelector("WIDEVANE", widevane, 7, hp.getWideVaneSetting()));
  toSend.replace("_ROOMTEMP_", String(hp.CelsiusToFahrenheit(hp.getRoomTemperature())) + "/" + String(hp.CelsiusToFahrenheit(hp.getTemperature())));
  toSend.replace("_REMOTETEMP_", createOptionSelector("REMOTETEMP", tempF, 31, String(hp.CelsiusToFahrenheit(hp.getRoomTemperature()))));
  toSend.replace("_MQTT_SERVER_", mqttServer);
  toSend.replace("_MQTT_PORT_", mqttPort);
  toSend.replace("_MQTT_USER_", mqttUser);
  toSend.replace("_MQTT_PWD_", mqttPwd);
  server.send(200, "text/html", toSend);
  delay(100);
}
/*
 * ********************************************************************************

 * ********************************************************************************
*/

bool change_states() {

  // update MQTT parameters
  if (server.hasArg("mqtt_form"))
  {
    if (server.hasArg("mqtt_server")) server.arg("mqtt_server").toCharArray(mqttServer, sizeof(mqttServer));
    if (server.hasArg("mqtt_port")) server.arg("mqtt_port").toCharArray(mqttPort, sizeof(mqttPort));
    if (server.hasArg("mqtt_user")) server.arg("mqtt_user").toCharArray(mqttUser, sizeof(mqttUser));
    if (server.hasArg("mqtt_pwd")) server.arg("mqtt_pwd").toCharArray(mqttPwd, sizeof(mqttPwd));
    writeConfigToDisk();
    delay(10000);
    ESP.reset();
  }

  // handle heatpump requests
  bool updated = false;
  if (server.hasArg("CONNECT")) {
    hp.connect(&Serial);
  }
  else if (server.hasArg("REMOTETEMP")) {
    console.println("Remote Room Temp set to " + server.arg("REMOTETEMP"));
    if (server.arg("REMOTETEMP") == "HP")
    {
      remoteTemp = 0;
      console.println("Reverting to HP internal temp sensor");
    }
    else
    {
      remoteTemp = hp.FahrenheitToCelsius(server.arg("REMOTETEMP").toInt());
      console.println(String(remoteTemp));
    }
    hp.setRemoteTemperature(remoteTemp);

  }
  else {
    if (server.hasArg("POWER")) {
      hp.setPowerSetting(server.arg("POWER"));
      updated = true;
    }
    if (server.hasArg("MODE")) {
      hp.setModeSetting(server.arg("MODE"));
      updated = true;
    }
    if (server.hasArg("TEMP")) {
      hp.setTemperature(hp.FahrenheitToCelsius(server.arg("TEMP").toInt()));
      updated = true;
    }
    if (server.hasArg("FAN")) {
      hp.setFanSpeed(server.arg("FAN"));
      updated = true;
    }
    if (server.hasArg("VANE")) {
      hp.setVaneSetting(server.arg("VANE"));
      updated = true;
    }
    if (server.hasArg("DIR")) {
      hp.setWideVaneSetting(server.arg("WIDEVANE"));
      updated = true;
    }
    hp.update();
  }
  return updated;
}
/*
 * ********************************************************************************

 * ********************************************************************************
*/

String encodeString(String toEncode) {
  toEncode.replace("<", "&lt;");
  toEncode.replace(">", "&gt;");
  toEncode.replace("|", "&vert;");
  return toEncode;
}
/*
 * ********************************************************************************

 * ********************************************************************************
*/

String createOptionSelector(String name, const String values[], int len, String value) {
  String str = "<select name='" + name + "'>\n";
  for (int i = 0; i < len; i++) {
    String encoded = encodeString(values[i]);
    str += "<option value='";
    str += values[i];
    str += "'";
    str += values[i] == value ? " selected" : "";
    str += ">";
    str += encoded;
    str += "</option>\n";
  }
  str += "</select>\n";
  return str;
}
/*
 * ********************************************************************************

 * ********************************************************************************
*/

void handleNotFound() {
  server.send ( 200, "text/plain", "URI Not Found" );
}

#endif
