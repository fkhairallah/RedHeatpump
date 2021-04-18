/************************************************************************************************

This class will implement a 'virtual console' that simulates the Serial debugger on an Arduino

it uses a telnet protocol on port 21


* (c) 2014 Deligent LLC - All rights reserved
*************************************************************************************************/

#ifndef _D_CONSOLE_
#define _D_CONSOLE_

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#define CMD_MAX_LENGTH 120


class dConsole : public Stream {

public:

	char commandString[CMD_MAX_LENGTH+1];
	char parameterString[CMD_MAX_LENGTH+1];


	dConsole();

	void enableSerial(HardwareSerial* serialPort, bool consoleMode);
	void disableSerial();
	void enableTelnet(int tcpPort);
	void disableTelnet();
	void enableUDP(IPAddress localIP, int udpPort);
	void disableUDP();

	bool check();

  void begin();

  void stop();

  bool isTelnetConnected();



  // Stream implementation

  int read();

  int available();

  int peek();



  // Print implementation

  size_t write(uint8_t val);

  using Print::write; // pull in write(str) and write(buf, size) from Print

  void flush();



private:

	char tempBuffer[CMD_MAX_LENGTH+1];
	int bufferCount;
	bool readFlag;

	int telnetPort;
	HardwareSerial* serial;
	bool debugMode;

	char broadcastIP[17];
	int udpPort;
	WiFiUDP udp;

	WiFiServer* server;
	WiFiClient client;

	void sendUDP(char* sentence);
	void trace(char* char_array);
	boolean disconnected();
	boolean parseCommand();

};



#endif


