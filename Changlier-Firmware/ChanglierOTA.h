//----------------------------------------------------------------------------------------
//
//	CHANGLIER Firmware
//						
//		Target MCU: DOIT ESP32 DEVKIT V1
//		Copyright:	2020 Michael Egger, me@anyma.ch
//		License: 	This is FREE software (as in free speech, not necessarily free beer)
//					published under gnu GPL v.3
//
//	  
//----------------------------------------------------------------------------------------

#ifndef __CHANCLIER_OTA
#define __CHANCLIER_OTA 1

#include "Changlier.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define WIFI_TIMEOUT		4000

extern String ssid;
extern String password;

extern boolean wifi_enabled;
extern WebServer server;

extern const char* serverIndex;
 
 
 void enable_wifi();
 
 
 #endif