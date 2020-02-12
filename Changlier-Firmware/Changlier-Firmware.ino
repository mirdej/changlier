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
#include <ESP32Servo.h>
//#include "AppleMidi.h"
#include <WiFiUdp.h>
#include "CircularBuffer.h"

//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

// .............................................................................Pins 

const char 	servo_pins[] 			= {32,33,25,26,27,14};
const char 	BTN_SEL 				= 3;	// Select button
const char 	BTN_UP 					= 1; // Up

const char NUM_SERVOS				= 6;

const char * udpAddress = "10.0.0.3";
const int udpPort = 44444;
WiFiUDP udp;


//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
Preferences                             preferences;
Timer									t;
Servo 									myservo[NUM_SERVOS];

CircularBuffer<byte, 100> queue;


// .............................................................................WIFI STUFF 
#define WIFI_TIMEOUT					4000
String 									hostname;
AsyncWebServer                          server(80);
//APPLEMIDI_CREATE_INSTANCE(WiFiUDP, AppleMIDI); // see definition in AppleMidi_Defs.h

char servo_val_raw[6];
long last_packet;
boolean isConnected;

char servo_address[NUM_SERVOS];
char servo_minimum[NUM_SERVOS];
char servo_maximum[NUM_SERVOS];
char servo_detach_address[NUM_SERVOS];

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

   if(preferences.getBytesLength("addresses") != NUM_SERVOS) {
    	Serial.println(F("Generating default adresses"));
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_address[i] = i+1;
    	}
    	preferences.putBytes("addresses",servo_address,NUM_SERVOS);
    } else {
        preferences.getBytes("addresses",servo_address,NUM_SERVOS);
    }
    
   if(preferences.getBytesLength("minima") != NUM_SERVOS) {
    	Serial.println(F("Generating default minima + maxima"));
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_minimum[i] = 0;
    		servo_maximum[i] = 180;
    	}
    	preferences.putBytes("minima",servo_minimum,NUM_SERVOS);
    	preferences.putBytes("maxima",servo_maximum,NUM_SERVOS);
    } else {
        preferences.getBytes("minima",servo_minimum,NUM_SERVOS);
    	preferences.getBytes("maxima",servo_maximum,NUM_SERVOS);
    }

   if(preferences.getBytesLength("detach") != NUM_SERVOS) {
    	Serial.println(F("Generating default detach adresses"));
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_detach_address[i] = i+7;
    	}
    	preferences.putBytes("detach",servo_detach_address,NUM_SERVOS);
    } else {
        preferences.getBytes("detach",servo_detach_address,NUM_SERVOS);
    }


	preferences.end();
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
 if(var == "SERVOS"){
 		String table = "";
 		
 		for (int i = 0; i < NUM_SERVOS; i++) {

 			table += "<tr>";
 			table += "<th scope=\"row\">" + String(i + 1) + "</th>";
      		table += "<td><input name =\"cc[" + String(i) + "]\" 		type=\"number\" 	maxlength=\"3\" size=\"3\" value=\""+  String(servo_address[i],DEC) +"\"></td>";
     		table += "<td><input  name =\"min[" + String(i) + "]\" 	type=\"number\" 	maxlength=\"3\" size=\"3\" value=\""+  String(servo_minimum[i],DEC) +"\"></td>";
			table += "<td><input  name =\"max[" + String(i) + "]\" 	type=\"number\" 	maxlength=\"3\" size=\"3\" value=\""+  String(servo_maximum[i],DEC) +"\"></td>";
			table += "<td><input  name =\"det[" + String(i) + "]\" 	type=\"number\" 	maxlength=\"3\" size=\"3\" value=\""+  String(servo_detach_address[i],DEC) +"\"></td>";
 			table += "<\tr>";
 		
 		}
        return table;
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
  		  
       server.on("/servos", HTTP_GET, [] (AsyncWebServerRequest *request) {
            String inputMessage;
                       //List all parameters
            int params = request->params();
            int n;
            int val;
            
            for(int i=0;i<params;i++){
              AsyncWebParameter* p = request->getParam(i);
					n = int(p->name().charAt(p->name().indexOf('[')+1))-48;
					val = p->value().toInt();
					if (val < 0) val = 0;
					if (val > 180) val = 180;
					
					if ((n >= 0) && (n < NUM_SERVOS)) {
	
						if (p->name().startsWith("cc")) { servo_address[n] = val; }
						 if (p->name().startsWith("min")) { servo_minimum[n] = val; }
						 if (p->name().startsWith("max")) { servo_maximum[n] = val; }
						 if (p->name().startsWith("det")) { servo_detach_address[n] = val; }
					}
            }
            
    		preferences.begin("changlier", false);
    		preferences.putBytes("addresses",servo_address,NUM_SERVOS);
			preferences.putBytes("minima",servo_minimum,NUM_SERVOS);
    		preferences.putBytes("maxima",servo_maximum,NUM_SERVOS);
    		preferences.putBytes("detach",servo_detach_address,NUM_SERVOS);
  			preferences.end();
    
			request->send(SPIFFS, "/index.html",  String(), false, processor);
		});
		
		            
       server.on("/hostname", HTTP_GET, [] (AsyncWebServerRequest *request) {
            String inputMessage;
            
           if (request->hasParam("hostname")) {
                inputMessage = request->getParam("hostname")->value();
                Serial.print("Change Hostname to: ");
                Serial.println(inputMessage);
                if (inputMessage.length() > 2) {
					hostname = inputMessage;
					preferences.begin("changlier", false);
					preferences.putString("hostname", hostname);
					preferences.end();
				}
            } 
            
			request->send(SPIFFS, "/index.html",  String(), false, processor);
        });
        
        
        server.begin();
    }
}

void service_servos(){
	int	 val;
	int i = 0;
	if(!queue.isEmpty()) {
		
		val  = queue.pop();
		val = map(val ,0,127,servo_minimum[i],servo_maximum[i]);
		myservo[i].write(val);
	}
/*	for (int i = 0; i < 6; i++) {
		val  = servo_val_raw[i];
		val = map(val ,0,127,servo_minimum[i],servo_maximum[i]);
		myservo[i].write(val);
	}
	*/
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
    digitalWrite(LED_BUILTIN,HIGH);

 	setup_read_preferences();
 	setup_web_server();

	for (int i = 0; i < 6; i++) {
		myservo[i].attach(servo_pins[i]);
	}
	
	udp.begin(udpPort);

 	t.every(2,service_servos);
}



//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop
 
void loop(){
	uint8_t buffer[50];

    t.update();

	udp.parsePacket();
  	char len = udp.read(buffer, 50);
	if( len > 0 ){
		queue.push(buffer[0]);
		last_packet = millis();
	}
	
    if (millis() - last_packet > 100) {
    	digitalWrite(LED_BUILTIN,LOW);
    }
}

