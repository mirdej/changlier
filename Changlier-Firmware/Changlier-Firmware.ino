#define VERSION "2020-02-12"

//----------------------------------------------------------------------------------------
//
//	CHANGLIER Firmware
//						
//		Target MCU: DOIT ESP32 DEVKIT V1
//		Copyright:	2019 Michael Egger, me@anyma.ch
//		License: 	This is FREE software (as in free speech, not necessarily free beer)
//					published under gnu GPL v.3
//
//----------------------------------------------------------------------------------------


#include <Preferences.h>
#include "Password.h"
#include "WiFi.h"
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include "ESPAsyncWebServer.h"
#include <ESP32Servo.h>
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
const int LOG_SIZE					= 500;

const char * udpAddress = "192.168.0.255";
const int udpPort = 44444;
const int WIFI_TIMEOUT		=			4000;


//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
Preferences                             preferences;
Servo 									myservo[NUM_SERVOS];

//CircularBuffer<byte, 100> queue;

String 									hostname;
AsyncWebServer                          server(80);
WiFiUDP 								udp;


char servo_val_raw[NUM_SERVOS];
long last_packet;
long servo_packet_interval;
long servo_packet_interval_avg;
long servo_packet_interval_min;
long servo_packet_interval_max;
long last_servo_service;
CircularBuffer<unsigned int, LOG_SIZE> 	servo_packet_interval_log;

long servo_service_interval = 4000; // microseconds
boolean isConnected;

char servo_minimum[NUM_SERVOS];
char servo_maximum[NUM_SERVOS];

//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION

//----------------------------------------------------------------------------------------
//																				Preferences

void read_preferences() {
    preferences.begin("changlier", false);

    hostname = preferences.getString("hostname");
    if (hostname == String()) { hostname = "changlier"; }
    Serial.print("Hostname: ");
    Serial.println(hostname);
    
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
    
 if(var == "STATS"){
 		String table = "Shortest: "+String(((float)servo_packet_interval_min/1000.),2);
		table += "ms | Mean: "+String((float)servo_packet_interval_avg/1000.,2);
		table += "ms | Longest: "+String((float)servo_packet_interval_max/1000.,2) + "ms";
		//reset stats
		servo_packet_interval_min = 0;
		servo_packet_interval_avg = 0;
		servo_packet_interval_max = 0;
        return table;
 }


 if(var == "LOG"){
 		String table = " ['Packet','Time'],";
		// the following ensures using the right type for the index variable
		using index_t = decltype(servo_packet_interval_log)::index_t;
		float ms;
		for (index_t i = 0; i < servo_packet_interval_log.size(); i++) {
			ms = (float)servo_packet_interval_log[i]/1000.;
			if (ms > 500.) ms = 500.;
			table +="[" + String(i) + ",";
			table += String(ms,2);
			table += "]";
			if ( i < servo_packet_interval_log.size()-1) 			table += ",";
		}
        return table;
 }

 if(var == "SERVOS"){
 		String table = "";
 		
 		for (int i = 0; i < NUM_SERVOS; i++) {

 			table += "<tr>";
 			table += "<th scope=\"row\">" + String(i + 1) + "</th>";
      		table += "<td>"+ String(myservo[i].read()) + "</td>";
     		table += "<td><input  name =\"min[" + String(i) + "]\" 	type=\"number\" 	maxlength=\"3\" size=\"3\" value=\""+  String(servo_minimum[i],DEC) +"\"></td>";
			table += "<td><input  name =\"max[" + String(i) + "]\" 	type=\"number\" 	maxlength=\"3\" size=\"3\" value=\""+  String(servo_maximum[i],DEC) +"\"></td>";
			table += "<td>"+ String(servo_val_raw[i], DEC) +"</td>";
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

        server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(SPIFFS, "/log.html",  String(), false, processor);
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
	
						 if (p->name().startsWith("min")) { servo_minimum[n] = val; }
						 if (p->name().startsWith("max")) { servo_maximum[n] = val; }
					}
            }
            
    		preferences.begin("changlier", false);
			preferences.putBytes("minima",servo_minimum,NUM_SERVOS);
    		preferences.putBytes("maxima",servo_maximum,NUM_SERVOS);
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
	for (int i = 0; i < NUM_SERVOS; i++) {
		val  = servo_val_raw[i];
		val = map(val ,0,127,servo_minimum[i],servo_maximum[i]);
		myservo[i].write(val);
	}
	last_servo_service = micros();
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

	read_preferences();
 	setup_web_server();

	for (int i = 0; i < NUM_SERVOS; i++) {
		myservo[i].attach(servo_pins[i]);
	}
	
	udp.begin(udpPort);
}

void calc_stats() {
	if (last_packet == 0) {
			last_packet = micros();
			return;
	}
	
	servo_packet_interval = micros() - last_packet;
	last_packet = micros();

	servo_packet_interval_log.push(servo_packet_interval);

	if (servo_packet_interval_avg == 0) {
		servo_packet_interval_avg = servo_packet_interval;
	} else {
		servo_packet_interval_avg = (15 * servo_packet_interval_avg + servo_packet_interval) / 16;
	}
			
	if (servo_packet_interval_min == 0) servo_packet_interval_min = servo_packet_interval;
	if (servo_packet_interval_max == 0) servo_packet_interval_max = servo_packet_interval;

	if (servo_packet_interval_min > servo_packet_interval) servo_packet_interval_min = servo_packet_interval;
	if (servo_packet_interval_max < servo_packet_interval) servo_packet_interval_max = servo_packet_interval;
}


//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop
 
void loop(){
	uint8_t buffer[50];


	udp.parsePacket();
  	char len = udp.read(buffer, 50);
	if( len > 0 ){
		digitalWrite(LED_BUILTIN,HIGH);
		
		if (buffer[0] == 's') {					// servo message
			calc_stats();
			int numbytes = len - 1;
			if (numbytes > NUM_SERVOS) numbytes = NUM_SERVOS;		// prevent reading garbage when receiving less bytes

			for (int i = 0; i < numbytes; i++) {
				servo_val_raw[i] = buffer[i + 1] - 1;
			}
		}
	}
	
    if ((micros() - last_packet)		 	> 200000) {					 	digitalWrite(LED_BUILTIN,LOW);}
	if ((micros() - last_servo_service) 	> servo_service_interval) { 	service_servos(); }

}

