#define VERSION "2019-12-03"

//----------------------------------------------------------------------------------------
//
//	CHANGLIER Firmware
//						
//		Target MCU: DOIT ESP32 DEVKIT V1
//		Copyright:	2019 Michael Egger, me@anyma.ch
//		License: 	This is FREE software (as in free speech, not necessarily free beer)
//					published under gnu GPL v.3
//
// !!!!!!!!!!!!!!!! 
// !!!!    Input functionality requires replacing the esp32-hal-uart.c
// !!!!  	files in Arduino/hardware/espressif/esp32/cores/esp32/
// !!!!!!!!!!!!!!!! 
//
//----------------------------------------------------------------------------------------


#include <Preferences.h>
#include "Timer.h"
#include "Password.h"
#include "WiFi.h"
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include "ESPAsyncWebServer.h"
#include <ArduinoOSC.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

OscWiFi osc;

#include <LXESP32DMX.h>


//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

// .............................................................................Pins 

const char 	servo_pins[] 			= {32,33,25,26,27,14};
const char 	BTN_SEL 				= 3;	// Select button
const char 	BTN_UP 					= 1; // Up
const char 	DMX_DIRECTION_PIN		= 4;
const char 	DMX_SERIAL_OUTPUT_PIN 	= 17;
const char 	DMX_SERIAL_INPUT_PIN 	= 16;

// ..................................................................................... SCREEN


#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 32 
#define OLED_RESET		 -1
Adafruit_SSD1306 					display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
Preferences                             preferences;
Timer									t;
Servo 									myservo[6];



// .............................................................................WIFI STUFF 
#define WIFI_TIMEOUT					4000
String 									hostname;
AsyncWebServer                          server(80);
long last_packet;
#define UDP_BUF_SIZE 127
char udp_packet[UDP_BUF_SIZE];

char servo_val_raw[6];

long packet_count;

//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION

//----------------------------------------------------------------------------------------
//																				Preferences

void setup_read_preferences() {
    preferences.begin("changlier", false);

    hostname = preferences.getString("hostname");
    if (hostname == String()) { hostname = "changlier"; }
    Serial.print("Hostname: ");
    Serial.println(hostname);

	preferences.end();
}

//----------------------------------------------------------------------------------------
//																				Restart


void restart() {
	display.clearDisplay();
	display.setCursor(0,0);
	display.setTextSize(2);	
	display.println(F("Restart...")); 
	display.display();
	delay(500);
	ESP.restart();
}
//----------------------------------------------------------------------------------------
//																			file functions

 String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

//----------------------------------------------------------------------------------------
//																process webpage template

// Replaces placeholders
String processor(const String& var){
  if(var == "HOSTNAME"){
        return hostname;
    }
  return String();
}
//----------------------------------------------------------------------------------------
//																				WebServer

void setup_web_server() {
   WiFi.begin(ssid, pwd);
    long start_time = millis();
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        if ((millis()-start_time) > WIFI_TIMEOUT) break;
	}

  	if (WiFi.status() == WL_CONNECTED) {
  	    Serial.print("Wifi connected. IP: ");
        Serial.println(WiFi.localIP());

        if (!MDNS.begin(hostname.c_str())) {
             Serial.println("Error setting up MDNS responder!");
        }
        Serial.println("mDNS responder started");

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(SPIFFS, "/index.html",  String(), false, processor);
        });
 
        server.on("/src/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(SPIFFS, "/src/bootstrap.bundle.min.js", "text/javascript");
        });
 
        server.on("/src/jquery-3.4.1.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(SPIFFS, "/src/jquery-3.4.1.min.js", "text/javascript");
        });
 
        server.on("/src/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(SPIFFS, "/src/bootstrap.min.css", "text/css");
        });
  		  
         server.on("/readADC", HTTP_GET, [] (AsyncWebServerRequest *request) {
            request->send(200, "text/text", "Hello");
        });
 
       server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
            String inputMessage;
            
            inputMessage = "Nothing done.";           
                        
            //List all parameters
            int params = request->params();
            for(int i=0;i<params;i++){
              AsyncWebParameter* p = request->getParam(i);
              if(p->isFile()){ //p->isPost() is also true
                Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
              } else if(p->isPost()){
                Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
              } else {
                Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
              }
            }
            
        if (request->hasParam("hostname")) {
                inputMessage = request->getParam("hostname")->value();
                hostname = inputMessage;
                preferences.begin("changlier", false);
            	preferences.putString("hostname", hostname);
                preferences.end();

            } 
            
            request->send(200, "text/text", inputMessage);
        });
        
        
        server.begin();
    }
}

void clear_display() {
 		display.clearDisplay();
		display.display();
}

void setup_welcome_screen() {
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.setRotation(2); 

 	display.clearDisplay();
	display.setTextSize(1);				
	display.setTextColor(WHITE);
	display.setCursor(14,0);		
	display.println(F("STEVE OCTANE TRIO"));
	display.setTextSize(2);				
	display.println(F(" CHANGLIER"));
	display.display();
	delay(4000);
	
	display.clearDisplay();
	display.setTextSize(1);		
	display.setCursor(40,0);		
	display.println(F("version"));
	display.setCursor(34,12);		
	display.println(F(VERSION));
	display.display();
	delay(1000);
}

void service_servos(){
	int	 val;
	for (int i = 0; i < 6; i++) {
		val  = servo_val_raw[i];
		val = map(val ,0,255,0,180);
		myservo[i].write(val);
	}
}
//----------------------------------------------------------------------------------------
//																				OSC

void setup_osc() {

	osc.begin(1234);
    
    // add callbacks...
	osc.subscribe("/print", [](OscMessage& m){
        Serial.print(packet_count); Serial.print(" ");
 		Serial.println();
 		display.clearDisplay();
 		display.setCursor(0,6);
 		display.setTextSize(2);				
		display.print(packet_count);
 		display.display();
     });
     
    osc.subscribe("/restart", [](OscMessage& m){
         restart();
     });
    
    osc.subscribe("/reset", [](OscMessage& m){
         packet_count = 0;
     });

	osc.subscribe("/servo", [](OscMessage& m){
        	digitalWrite(LED_BUILTIN,HIGH);
            last_packet = millis();
            packet_count++;
            for (int i = 0; i < 6; i++) {
				servo_val_raw[i] = m.arg<int>(i);
            }
    });
}
//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup(){
    Serial.begin(115200);
 
    if(!SPIFFS.begin()){
         Serial.println("An Error has occurred while mounting SPIFFS");
         return;
    }
    
    pinMode(LED_BUILTIN,OUTPUT);
 
 	setup_read_preferences();
 	setup_web_server();
	setup_welcome_screen();
	setup_osc();
	
	for (int i = 0; i < 6; i++) {
		myservo[i].attach(servo_pins[i]);
	}

	t.every(2,service_servos);
}



//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop
 
void loop(){
    t.update();
	osc.parse(); // should be called very often 
    if (millis() - last_packet > 300) {
    	digitalWrite(LED_BUILTIN,LOW);
    }
}