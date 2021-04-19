/**************************************************************************************


  This class will implement a 'virtual console' that simulates the Serial debugger on an Arduino

  it uses a telnet protocol on port 21


  (c) 2014 Deligent LLC - All rights reserved
  ***************************************************************************************/
#include "dConsole.h"


dConsole::dConsole()
{
	serial = NULL;
	telnetPort = 0;
	server = NULL;
	udpPort = 0;
	readFlag = false;
	tempBuffer[0] = 0;
	commandString[0] = 0;
	parameterString[0] = 0;
	bufferCount = 0;
}


void dConsole::enableSerial(HardwareSerial* serialPort, bool consoleMode)
{
	serial = serialPort;
	serial->begin(115200);
	serial->flush();
	readFlag = consoleMode;
	tempBuffer[0] = 0;
	bufferCount = 0;
	println("[Console is activated on Serial Port]");
}
void dConsole::disableSerial()
{
	if (serial)
	{
		this->println("[Console is deactivate on Serial port]");
		delay(200);
		serial = NULL;
	}
}

void dConsole::enableTelnet(int tcpPort)
{
	if (telnetPort) // Telnet is already enabled
	{
		this->println("ERR: Attempting to open second telnet port");
		return;
	}
	telnetPort = tcpPort;
	if (telnetPort)
	{
		server = new WiFiServer(telnetPort); // telnet server
		server->begin();
		this->println("[Console is activated on Telnet port]");
	}
	tempBuffer[0] = 0;
	bufferCount = 0;

}
void dConsole::disableTelnet()
{
	if (server)
	{
		server = NULL;
		telnetPort = 0;
		this->println("[Console is deactivate on telnet port]");

	}
}

void dConsole::closeTelnetConnection()
{
	if (client) client.stop();
}

void dConsole::enableUDP(IPAddress localIP, int port)
{
	udpPort = port;
	sprintf(broadcastIP, "%d.%d.%d.255", localIP[0], localIP[1], localIP[2]);
	this->print("UDP: ");
	this->println(broadcastIP);
}
void dConsole::disableUDP()
{
	udpPort = 0;
	this->println("[Console is deactivate on UDP port]");
}

// Stream -- Parent class implementation

void dConsole::begin() {

	if (serial) {
		serial->begin(115200);
		println("[Console Serial Port initialized to 115200]");
	}
	if (telnetPort)
	{
		server = new WiFiServer(telnetPort); // telnet server
		server->begin();
		println("[Console is activated on Telnet port]");
	
	}

	tempBuffer[0] = 0;
	bufferCount = 0;


}



void dConsole::stop() {

  server->stop();

}

// checks if we have an active telent connection
bool dConsole::isTelnetConnected() {

    if (telnetPort)
    {
		if (server->status() == CLOSED) return false;

		if (!client) client = server->available();
	
		if (client) {

		  if (client.connected()) return true;
		}
	}
	return false;
}


 boolean dConsole::disconnected() {


    // FIRST: check for Telnet connection
    if (telnetPort)
    {
		if (server->status() == CLOSED) return true;

		// if we don't have a client, check to see if we have one.
		// then flush the queue so we don't get weird characters
		if (!client) 
		{
			client = server->available();
			client.flush();
		}
	

		if (client) {

		  if (client.connected()) return false;

		  // client is not connected, stop and get a new one
		  client.stop();
		  client = server->available();
		  client.flush();

		}
	}

    if (serial) return false; // also connected if serial is enabled

	return true;
}


int dConsole::read() {

  if (disconnected()) return -1;

  if (client.connected()) 
	return client.read();
  else
	return serial->read();

}



int dConsole::available() {

  if (this->disconnected())  return 0;

  if (client) if (client.available()) return client.available();
  if (serial) if (serial->available()) return serial->available();

  return 0;

}



int dConsole::peek() {

  if (disconnected())  return -1;

  return 0;

}


size_t dConsole::write(uint8_t val) {
size_t result=0;

  if (disconnected()) return 1;

	if (serial) result = serial->write(val);

	if (client.connected()) result = client.write(val);

  return result;

}



void dConsole::flush() {

  if (disconnected()) return;

  serial->flush();

  client.flush();

}


bool dConsole::check()
{
	// check serial port
	if ((serial) && (readFlag))
	{
		while (serial->available()) {
			char c = serial->read();
			yield(); // yield back to the OS

			//printf("%i\r\n",c);
			if (c == '\x08') // backspace
			{
				if (bufferCount > 0)
				{
					tempBuffer[--bufferCount] = 0;
				}
				serial->write(c);
				continue;
			}
			if (c == '\x15') // Control U -- erase entire line
			{
				bufferCount = 0;
				tempBuffer[bufferCount] = 0;
				serial->println();
				continue;
			}

			if (c > 127) continue; // we sometimes start with weird characters
			if (c == '\r') continue; // ignore CR
			if ( (c == '\n') || (bufferCount >= CMD_MAX_LENGTH) ) // LF is a command or max length
			{
				return parseCommand();

			}
			else {
				serial->write(c);
				tempBuffer[bufferCount++] = c;
				tempBuffer[bufferCount] = 0;
				//println(tempBuffer);
			}
			
		}
	}

	// now check telnet if a port has been defined
	if (telnetPort)
	{
		if (!client.connected()) {
			if (server)
			{
				// server is defined try to connect to a new client
				client = server->available();

				if (client)
				{
					println("Connected to [RED] debug console");
					println("'?' for more, 'exit' to exit");
					print("[RED]> ");

				}
			}
		}
		else // client is connected
		{
			if (client.status() == CLOSED) return false;

			// read data from the connected client
			while (client.available()) {
				char c = client.read();
				yield();	// yield back to the OS


				if (c == '\x08') // backspace
				{
					if (bufferCount > 0)
					{
						tempBuffer[--bufferCount] = 0;
					}
					continue;
				}
				if (c == '\x15') // Control U -- erase entire line
				{
					bufferCount = 0;
					tempBuffer[bufferCount] = 0;
					continue;
				}

				if (c == '\r') continue; // ignore CR
				if ((c == '\n') || (bufferCount >= CMD_MAX_LENGTH) )  // LF is a command
				{
					// we have a full line--> exit true
					return parseCommand();
				}
				else 
				{
					tempBuffer[bufferCount++] = c;
					tempBuffer[bufferCount] = 0;
				}
			}
		}
	}
	return false;  // no command
}

void dConsole::trace(char* char_array)
{


	if (serial) serial->print(char_array);
	if (client.connected()) client.print(char_array);
	if (udpPort) sendUDP(char_array);
}

void dConsole::sendUDP(char* sentence)
{
  udp.beginPacket(broadcastIP, udpPort);
  udp.write(sentence);
  udp.endPacket();
}


// parse line typed in into a command and a parameter

boolean dConsole::parseCommand()
{
	char * pch;

	// extract the command
	pch = strtok (tempBuffer," \t");
	if (pch != NULL)
		strcpy(commandString, pch);
	else
		strcpy(commandString, "");

	// see if there are any parameters
	pch = strtok(NULL," \t");
	if (pch != NULL) {
		strcpy(parameterString,pch);
	}
	else
		{
			parameterString[0] = 0;
		}
		
	tempBuffer[0] = 0;
	bufferCount = 0;

	return true;
}