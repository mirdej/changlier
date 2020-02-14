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
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

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

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"


//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
Preferences                             preferences;
Servo 									myservo[NUM_SERVOS];

String 									hostname;

unsigned char servo_val_raw[NUM_SERVOS];
unsigned char servo_detach;

unsigned long last_packet;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;


long servo_service_interval = 2000; // microseconds
long last_servo_service; // microseconds
boolean isConnected;

unsigned char servo_minimum[NUM_SERVOS];
unsigned char servo_maximum[NUM_SERVOS];


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



//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup(){
    Serial.begin(115200);

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
}



//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop
 
void loop(){
    if ((micros() - last_packet)		 	> 200000) {					 	digitalWrite(LED_BUILTIN,LOW);}
	if ((micros() - last_servo_service) 	> servo_service_interval) { 	service_servos(); }
}

