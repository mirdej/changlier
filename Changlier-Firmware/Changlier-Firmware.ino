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
#include <Timer.h>
#include <FastLED.h>

//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES


#define SYSEX_NOP				0
#define SYSEX_NAMECHANGE		22
#define SYSEX_SET_MINIMUM		25
#define SYSEX_SET_MAXIMUM		26
#define SYSEX_CLEAR_MIN_MAX		28
#define SYSEX_INVERT_MIN_MAX	29

#define SYSEX_SEND_SERVODATA	30

#define SYSEX_SERVODATA			50

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"
// .............................................................................Pins 


const char 	servo_pin[] 			= {32,33,25,26,27,14};
const char  note_pin[]				= {22,21,23,19};
const char	PIN_PIXELS				= 13;
const char 	NUM_SERVOS				= 6;
const char	NUM_NOTES				= 4;
const char	NUM_PIXELS				= 6;



//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
Preferences                             preferences;
Servo 									myservo[NUM_SERVOS];
Timer									t;

CRGB                                    pixels[NUM_PIXELS];
CHSV									colors[NUM_PIXELS];

String 									hostname;

unsigned char servo_val_raw[NUM_SERVOS];
unsigned char servo_detach;

unsigned long last_packet;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

boolean isConnected;

unsigned char servo_minimum[NUM_SERVOS];
unsigned char servo_maximum[NUM_SERVOS];

uint8_t midiPacket[] = {
   0x80,  // header
   0x80,  // timestamp, not implemented 
   0x00,  // status
   0x3c,  // 0x3c == 60 == middle c
   0x00   // velocity
};


//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION
void send_servo_data(int channel) {
	const int send_servo_data_reply_length = 15;
	uint8_t packet[send_servo_data_reply_length];
	
//	Serial.print("Send: ");
	
	packet[0] = 0x80;  // header
	packet[1] = 0x80;  // timestamp, not implemented 
	packet[2] = 0xF0;  // SYSEX
	packet[3] = 0x7D;  // Homebrew Device
	
	packet[4] = SYSEX_SERVODATA;
	packet[5] = 7;					//length
	packet[6] = channel;
	packet[7] = servo_minimum[channel] >> 1;
	packet[8] = servo_minimum[channel] & 1;
	packet[9] = myservo[channel].read() >> 1;
	packet[10] = myservo[channel].read() & 1;
	packet[11] = servo_maximum[channel] >> 1;
	packet[12] = servo_maximum[channel] & 1;
	
	packet[13] = 0x80; // fake checksum
	packet[14] = 0xF7; 	// end of sysex

   pCharacteristic->setValue(packet, send_servo_data_reply_length); // packet, length in bytes)
   pCharacteristic->notify();
}


void led_control(char idx, char val) {
	if (idx < 7 ) return;
	if (idx == 7) { 					// channel 8:			global hue
		for (int i = 0; i < NUM_PIXELS; i++) {
			colors[i].hue = 2 * val;
		}
	} else if (idx == 8) { 					// channel 9:			global saturation
		for (int i = 0; i < NUM_PIXELS; i++) {
			colors[i].saturation = 2 * val;
		}
	} else if (idx == 9) { 					//channel 10:  			global brightness
		for (int i = 0; i < NUM_PIXELS; i++) {
			colors[i].value = 2 * val;
		}					
	} else {								//channels 11-16:		individual brightness
		if ((idx - 10) < NUM_PIXELS) {
			colors[(idx - 10)].value = 2 * val;
		} 
	}
}


void set_limits(char channel) {
	if (channel < NUM_SERVOS) {
		Serial.print("Servo ");
		Serial.print(channel + 1, DEC);
		Serial.print(" limits: ");
		Serial.print(servo_minimum[channel], DEC);
		Serial.print(" - ");
		Serial.print(servo_maximum[channel], DEC);
		Serial.println();
	}
	preferences.begin("changlier", false);
	preferences.putBytes("minima",servo_minimum,NUM_SERVOS);
	preferences.putBytes("maxima",servo_maximum,NUM_SERVOS);
	preferences.end();
}

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
	      last_packet = millis();
			digitalWrite(LED_BUILTIN,HIGH);
//------------------------------						// Control Change
		if ((rxValue[2] >> 4 ) == 0x0B) {				
			int idx = rxValue[3]-1;
			char val = rxValue[4];
			if (val > 127) val = 127;
			if (idx >= 0) {
				if (idx < NUM_SERVOS) {					// channels 1 - 6:	 	control servos
					servo_val_raw[idx] = val;
				} else if (idx == 6) { 
					servo_detach = val;					// channel 7: 			global servo detach 
					if (val > 64) servo_detach = 0xFF;
				} else {
					led_control(idx,val);
				}
			}
		} 
		
//------------------------------						// Note ON
		if ((rxValue[2] >> 4 ) == 0x09) {				
			int idx = rxValue[3]-1;
			char val = rxValue[4];
			if (val > 127) val = 127;
			if (idx >= 10 )led_control(idx,val);
		} 
//------------------------------						// Note OFF
		if ((rxValue[2] >> 4 ) == 0x08) {				
			int idx = rxValue[3]-1;
			char val = rxValue[4];
			if (val > 127) val = 127;
			if (idx >= 10 )led_control(idx,0);
		} 

//------------------------------					// SYSEX	
		if (rxValue[2] == 0xF0) {					
			if (rxValue[3] == 0x7D) {				// Homebrew Device

				char command = rxValue[4];
				char len = rxValue[5];
				char channel = rxValue[6]-1;
				String new_name = "";

				switch (command) {
					case SYSEX_NOP:
						Serial.print("NOP");
						break;								
					case SYSEX_NAMECHANGE:
						if (len > rxValue.length() - 8) {
							Serial.println("PARSE ERROR: Incorrect length");
							Serial.print(len,DEC);
							Serial.print(" ");
							Serial.print(rxValue.length(),DEC);

						} else {
							for (int i = 0; i < len; i++) {
								char in = rxValue[i + 6];
								if ((in > 31) && (in < 123)) {
									new_name += in;
								} else {
									Serial.println("PARSE ERROR: Forbidden char");
								}
							}
							Serial.print("Change name to: ");
							Serial.println(new_name);
							preferences.begin("changlier", false);
							preferences.putString("hostname",new_name);
							preferences.end();
							
						}
						break;
					case SYSEX_CLEAR_MIN_MAX:
						if (channel < NUM_SERVOS) {
								servo_minimum[channel] = 0;
								servo_maximum[channel] = 180;
								set_limits(channel);
						}
						break;
					case SYSEX_SET_MINIMUM:
						if (channel < NUM_SERVOS) {
							servo_minimum[channel] = myservo[channel].read();
							set_limits(channel);
						}
						break;
					case SYSEX_SET_MAXIMUM:
						if (channel < NUM_SERVOS) {
							servo_maximum[channel] = myservo[channel].read();
							set_limits(channel);
						}
						break;
					case SYSEX_INVERT_MIN_MAX:
						if (channel < NUM_SERVOS) {
							char old_min = servo_minimum[channel];
							servo_minimum[channel] = servo_maximum[channel];
							servo_maximum[channel] = old_min;
							set_limits(channel);
						}
						break;
					case SYSEX_SEND_SERVODATA:
						if (channel < NUM_SERVOS) {
							send_servo_data(channel);
						}
						break;
					default:
						break;
					}
				}
			}
 	   }
    }
};

//----------------------------------------------------------------------------------------
//																				Preferences

void read_preferences() {
    preferences.begin("changlier", false);
	
    hostname = preferences.getString("hostname");
    if (hostname == String()) { hostname = "Bebe Changlier"; }
    
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
//																				NOTES

void check_buttons() {
	static char old_button[NUM_NOTES];
	for (int i = 0; i < NUM_NOTES; i++) {
		char button = digitalRead(note_pin[i]);
		if (old_button[i] != button) {
			old_button[i] = button;
			 midiPacket[3] 	= 5+i;

			if (button) {
				midiPacket[2] = 0x80; // note off, channel 0
				midiPacket[4] = 0;  // velocity
			} else {
				midiPacket[2] = 0x90; // note on, channel 0
				midiPacket[4] = 127;    // velocity
			}
			
			pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
			pCharacteristic->notify();
		}
	}
}

//----------------------------------------------------------------------------------------
//																				LEDS

void update_leds() {
	for (int i = 0; i < NUM_PIXELS; i++) {
		pixels[i] = colors[i];
	}
	FastLED.show();
}

//----------------------------------------------------------------------------------------
//																				SERVOS

void service_servos(){
	int	 target_val;
	for (int i = 0; i < NUM_SERVOS; i++) {
		if (servo_val_raw[i] < 128) {
			if (servo_detach & (1 << i)) {
				if (myservo[i].attached()) {
					myservo[i].detach();
				}
			} else {
					if (!myservo[i].attached()) {
							myservo[i].attach(servo_pin[i]);
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
	
	FastLED.addLeds<NEOPIXEL, PIN_PIXELS>(pixels, NUM_PIXELS);
	
	for (int hue = 0; hue < 360; hue++) {
    	fill_rainbow( pixels, NUM_PIXELS, hue, 7);
	    delay(3);
    	FastLED.show(); 
  	}
  
	for (int i = 0; i< NUM_PIXELS; i++) {
		colors[i].hue = 42; // yellow
		colors[i].saturation = 160;
		colors[i].value = 0;
	}

	for (int i = 0; i< NUM_SERVOS; i++) {
		servo_val_raw[i] = 255;
	}
	for (int i = 0; i< NUM_NOTES; i++) {
		pinMode(note_pin[i], INPUT_PULLUP);
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

	t.every(4,service_servos);
	t.every(20,check_buttons);
	t.every(20,update_leds);
	digitalWrite(LED_BUILTIN,LOW);
}



//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop
 
void loop(){
	t.update();
    if ((millis() - last_packet) > 200) {digitalWrite(LED_BUILTIN,LOW);}
}

