# RedHeatpump

[RED]Heatpump is an ESP8266-based IOT that connects Mitsubish heatpumps to OpenHab

   This code is design to interface to a Mitsubishi Split Heat Pump through CN105 conenctor
     Original work by https://nicegear.nz/blog/hacking-a-mitsubishi-heat-pump-air-conditioner/
     protocol details here https://github.com/hadleyrich/MQMitsi
     Code based on https://github.com/SwiCago/

  Modifications have been made to allow easy interface with OpenHab smart home system through MQTT server

  This code provides WIFI & MQTT configuration through the use of WIFI manager. Initial setup allow
  configuration of WIFI network and MQTT server

  It provides OTA functionality.

  It provides web access to the heatpump as well as the ability to change MQTT server.

  It offers console services through serial prot (initially until heatpump is booted), telnet
  and on UDP port 10110

  It interfaces with MQTT server through 4 Topics:
  1. heatpump/location/default - send current settings
  2. heatpump/location/set - listens for commands such as settings changes, remote temp, ...
  3. heatpump/location/status - sends current room temp and operating conditions
  4. heatpump/location/timers - shows any timers the hp is using

  In addition there are two more topics the allows you to debug the hp:
  1. heatpump/location/debug -- forwards all packets exchanged with hp
  2. heatpump/location/debug/set -- received command (on/off) to control debug mode


# Hardware Notes

The implementation in Rye Manor uses Adafruit Huzzah board



![The circuit is simple -- just a TTL RS232 interface](media\schematic.jpg)


![Pin location on the HP controller board](media\pinlocation.png)

    - GPIO-0 must be tied to ground for programming
    - GPIO-0 floats to run program
    - GPIO-0 runs Red LED on Huzzah
    - GPIO-2 is tied to Blue Led (*NOT* a PWM pin)
    - GPIO-13 is RESERVED
    - Serial port is used to communicate with heatpump and cannot be used for debugging
