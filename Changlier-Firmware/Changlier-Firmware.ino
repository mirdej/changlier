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
#include "WiFi.h"
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include "ESPAsyncWebServer.h"
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include <WiFiAP.h>
#include "Timer.h"

//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

// .............................................................................Pins 


const char 	servo_pins[] 			= {32,33,25,26,27,14};
const char 	BTN_SEL 				= 3;	// Select button
const char 	BTN_UP 					= 1; // Up

const char NUM_SERVOS				= 6;
const int LOG_SIZE					= 200;
const int BUF_SIZE					= 100;

const int SERVO_MAX_STEP			= 2;


const int WIFI_TIMEOUT		=			8000;

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"


//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
Preferences                             preferences;
Servo 									myservo[NUM_SERVOS];
Timer 									t;

//CircularBuffer<byte, 100> queue;

String 									hostname;
AsyncWebServer                          server(80);



int 									network_count;

unsigned char servo_val_raw[NUM_SERVOS];
unsigned char servo_detach;

unsigned long last_packet;
unsigned long servo_packet_interval;
unsigned long servo_packet_interval_avg;
unsigned long servo_packet_interval_min;
unsigned long servo_packet_interval_max;
unsigned long last_servo_service;
unsigned long last_buffer_service;


BLECharacteristic *pCharacteristic;
bool deviceConnected = false;


long servo_service_interval = 2000; // microseconds
boolean isConnected;

unsigned char servo_minimum[NUM_SERVOS];
unsigned char servo_maximum[NUM_SERVOS];

const char* 							ssid_perdu = "Changlier Perdu";
String									ssid ="";
String									pass="";


//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {

    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
	      last_packet = micros();
		if ((rxValue[2] >> 4 ) == 0x0B) {
			int idx = rxValue[3]-1;
			char val = rxValue[4];
			if (val > 127) val = 127;
			
			if (idx >= 0 && idx < NUM_SERVOS) {
				servo_val_raw[idx] = val;
			} else if (idx == 6) servo_detach = val;
		}
      }
    }
};

//----------------------------------------------------------------------------------------
//																				Preferences

void read_preferences() {
    preferences.begin("changlier", false);
	
	ssid = preferences.getString("ssid");
	pass = preferences.getString("pass");
    //Serial.print(("SSID: "); //Serial.print(ln(ssid);

    hostname = preferences.getString("hostname");
    if (hostname == String()) { hostname = "bebe_changlier"; }
    
   if(preferences.getBytesLength("minima") != NUM_SERVOS) {
    	//Serial.print(ln(F("Generating default minima + maxima"));
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
  //Serial.print(f("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    //Serial.print(ln("- empty file or failed to open file");
    return String();
  }
  //Serial.print(ln("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  //Serial.print(ln(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  //Serial.print(f("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    //Serial.print(ln("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    //Serial.print(ln("- file written");
  } else {
    //Serial.print(ln("- write failed");
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
    
     if(var == "NETWORKS"){
  		String list = "";
		for (int i = 0; i < network_count; ++i) {
			list += "<div>";
			list += "<input type=\"radio\" name=\"r"+String(i)+"\" id=\"r"+String(i)+"\" value=\"1\"";
  			list += "<label class=\"form-check-label\" for=\"r"+String(i)+"\">";
			list += String(i + 1) + " : " + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + ")" + ((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
			list +=" </label></div>";
		}
	    return list;
    }
    return String();

  return String();
}
//----------------------------------------------------------------------------------------
//																				WebServer

void setup_web_server() {

	if (!MDNS.begin(hostname.c_str())) {
		 //Serial.print(ln("Error setting up MDNS responder!");
	}
	//Serial.print(ln("mDNS responder started");

    //Serial.print(("Hostname: ");
    //Serial.print(ln(hostname);

	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(SPIFFS, "/index.html",  String(), false, processor);
	});


	server.on("/ip", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(200, "text/plain",WiFi.localIP().toString());
	});

	server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(SPIFFS, "/log.html",  String(), false, processor);
	});


	server.on("/forget", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(SPIFFS, "/restart.html",  String(), false, processor);
			preferences.begin("changlier", false);
			preferences.putString("ssid", String());
			preferences.putString("pass", String());
			preferences.end();
		delay(2000);
		ESP.restart();
	});
	
	server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(SPIFFS, "/restart.html",  String(), false, processor);
		delay(2000);
		ESP.restart();
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
			//Serial.print(("Change Hostname to: ");
			//Serial.print(ln(inputMessage);
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

//----------------------------------------------------------------------------------------
//																				Login Server

void setup_login_server() {
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(SPIFFS, "/login.html",  String(), false, processor);
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

    server.on("/login", HTTP_GET, [] (AsyncWebServerRequest *request) {
            String inputMessage;
                       //List all parameters
            int params = request->params();
            
            if (request->hasParam("pass")) {
				inputMessage = request->getParam("pass")->value();
				//Serial.print(("Password: ");
				//Serial.print(ln(inputMessage);
			}
			
			int net;
			for(int i=0;i<params;i++){
              AsyncWebParameter* p = request->getParam(i);
              if (p->value().toInt() == 1) {
				//Serial.print(("Network: ");
				//Serial.print(ln(p->name());
				net = p->name().substring(1).toInt();
				//Serial.print(ln(WiFi.SSID(net));
              }
              
			}
			
			preferences.begin("changlier", false);
			preferences.putString("ssid", WiFi.SSID(net));
			preferences.putString("pass", inputMessage);
			preferences.end();

			request->send(SPIFFS, "/restart.html",  String(), false, processor);
			delay(2000);
			ESP.restart();
	});
	
	server.begin();
}

//----------------------------------------------------------------------------------------
//																				SERVOS

void service_servos(){
	last_servo_service = micros();
	int	 target_val;
	for (int i = 0; i < NUM_SERVOS; i++) {
		if (servo_val_raw[i] < 128) {
			if (servo_detach & (1 << i)) {
				if (myservo[i].attached()) {
					myservo[i].detach();
				}
			} else {
					if (!myservo[i].attached()) {
							myservo[i].attach(servo_pins[i]);
					} 
					target_val  = servo_val_raw[i];
					target_val = map(target_val ,0,127,servo_minimum[i],servo_maximum[i]);
					myservo[i].write(target_val);
				}	
			}	
	}
}

//----------------------------------------------------------------------------------------
//																				Transmission Stats


void calc_stats() {
	if (last_packet == 0) {
			last_packet = micros();
			return;
	}
	
	servo_packet_interval = micros() - last_packet;
	last_packet = micros();

	if (servo_packet_interval_avg == 0) {
		servo_packet_interval_avg = servo_packet_interval;
	} else {
		servo_packet_interval_avg = (31 * servo_packet_interval_avg + servo_packet_interval) / 32;
	}
			
	if (servo_packet_interval_min == 0) servo_packet_interval_min = servo_packet_interval;
	if (servo_packet_interval_max == 0) servo_packet_interval_max = servo_packet_interval;

	if (servo_packet_interval_min > servo_packet_interval) servo_packet_interval_min = servo_packet_interval;
	if (servo_packet_interval_max < servo_packet_interval) servo_packet_interval_max = servo_packet_interval;
}


//----------------------------------------------------------------------------------------
//																				Blink

void blink() {
	digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));
}



//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup(){
    Serial.begin(115200);
 
    if(!SPIFFS.begin()){
         //Serial.print(ln("An Error has occurred while mounting SPIFFS");
         return;
    }
    
    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN,HIGH);
	
	
	for (int i = 0; i< NUM_SERVOS; i++) {
		servo_val_raw[i] = 255;
	}
	read_preferences();
	
	
	  BLEDevice::init(hostname.c_str());
    
  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    BLEUUID(CHARACTERISTIC_UUID),
    BLECharacteristic::PROPERTY_READ   |
    BLECharacteristic::PROPERTY_WRITE  |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_WRITE_NR
  );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());
	pCharacteristic->setCallbacks(new MyCallbacks());


  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  pAdvertising->start();
	
	
	if (ssid != String()) {	
		// try to connect to stored network
		WiFi.begin(ssid.c_str(), pass.c_str());
    	long start_time = millis();
		while (WiFi.status() != WL_CONNECTED) { 
			delay(500); 
        	if ((millis()-start_time) > WIFI_TIMEOUT) break;
        }
	}

  	if (WiFi.status() == WL_CONNECTED) {
  	    //Serial.print(("Wifi connected. IP: ");
        //Serial.print(ln(WiFi.localIP());
        
		setup_web_server();
		
		for (int i = 0; i< NUM_SERVOS; i++) {
			myservo[i].attach(servo_pins[i]);
		}

		
	} else {
		WiFi.mode(WIFI_STA);
		WiFi.disconnect();

		//Serial.print(ln("scan start");
		// WiFi.scanNetworks will return the number of networks found
		int n = WiFi.scanNetworks();
		network_count = n;
		//Serial.print(ln("scan done");
	
		// You can remove the password parameter if you want the AP to be open.
		WiFi.softAP(ssid_perdu);
		IPAddress myIP = WiFi.softAPIP();
		//Serial.print(("AP IP address: ");
		//Serial.print(ln(myIP);
	
		if (!MDNS.begin("changlier")) {
			//Serial.print(ln("Error setting up MDNS responder!");
		}
		//Serial.print(ln("mDNS responder started");

	    //Serial.print(("Hostname: changlier");

	
		if (n == 0) {
			//Serial.print(ln("no networks found");
		} else {
			//Serial.print((n);
			//Serial.print(ln(" networks found");
			for (int i = 0; i < n; ++i) {
				// Print SSID and RSSI for each network found
				//Serial.print((i + 1);
				//Serial.print((": ");
				//Serial.print((WiFi.SSID(i));
				//Serial.print((" (");
				//Serial.print((WiFi.RSSI(i));
				//Serial.print((")");
				//Serial.print(ln((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
				delay(10);
			}
		}
	
		setup_login_server();
		t.every(200,blink);
	
	}






	//t.every(100,printraw);
}



void printraw(){
	if(servo_val_raw[0] < 128) {
		for (int i = 0; i < 3; i++ ) {
			//Serial.print((servo_val_raw[i],DEC);
			//Serial.print((" ");
		}
		//Serial.print(ln();
	}
}


//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop
 
void loop(){

	t.update();

    if ((micros() - last_packet)		 	> 200000) {					 	digitalWrite(LED_BUILTIN,LOW);}
	if ((micros() - last_servo_service) 	> servo_service_interval) { 	service_servos(); }
}

