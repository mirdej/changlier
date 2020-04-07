const char * version = "2020-04-07.0";

//----------------------------------------------------------------------------------------
//
//	CHANGLIER Firmware
//						
//		Target MCU: DOIT ESP32 DEVKIT V1
//		Copyright:	2019 Michael Egger, me@anyma.ch
//		License: 	This is FREE software (as in free speech, not necessarily free beer)
//					published under gnu GPL v.3
//
//		DMX Library: https://github.com/luksal/ESP32-DMX-RX
//		Attention buffer overflow bug on line 134
//
//----------------------------------------------------------------------------------------

#include <esp32-dmx-rx.h>

#include <Preferences.h>
//#include <ESP32Servo.h>		//version 0.6.3
#include "ServoEasing.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Timer.h>
#include <FastLED.h>

//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

#define	DMX_DETACH_TIME			10
#define	DMX_CHANNELS			20

#define SYSEX_NOP				0

#define SYSEX_GET_VERSION		10
#define SYSEX_GET_DMX_ADDRESS	11
#define SYSEX_GET_DEBOUNCE		12

#define SYSEX_NAMECHANGE		22
#define SYSEX_SET_DMX_ADDRESS	23
#define SYSEX_SET_SERVOSETTINGS	24
#define SYSEX_SET_MINIMUM_HERE		25
#define SYSEX_SET_MAXIMUM_HERE		26
#define SYSEX_CLEAR_MIN_MAX		28
#define SYSEX_INVERT_MIN_MAX	29

#define SYSEX_SEND_SERVODATA		30
#define SYSEX_SEND_SERVOSETTINGS	31

#define SYSEX_SET_MINIMUM		40
#define SYSEX_SET_MAXIMUM		41

#define SYSEX_SERVODATA			50
#define SYSEX_VERSION_DATA		51
#define SYSEX_SERVOSETTINGS		52
#define SYSEX_DMX_ADDRESS		53
#define SYSEX_DEBOUNCE			54
#define SYSEX_SET_DEBOUNCE			55

#define SYSEX_GET_PARAM		90
#define SYSEX_PARAM_DATA	91
#define SYSEX_SET_PARAM		92

const int PARAM_min = 2;
const int PARAM_max = 3;
const int PARAM_init = 4;
const int PARAM_detach_lo = 5;
const int PARAM_detach_hi = 6;
const int PARAM_ease_func = 7;
const int PARAM_speed = 8;
const int PARAM_ease_distance = 9;
const int PARAM_channel = 10;
const int PARAM_reset_all = 11;
const int PARAM_battery = 12;


#define DEFAULT_MINIMUM		0
#define DEFAULT_MAXIMUM		180
#define DEFAULT_MINIMUM_DETACH 63
#define DEFAULT_MINIMUM_DETACH 63
#define DEFAULT_INIT	90
#define DEFAULT_EASE	0
#define DEFAULT_SPEED	120
#define DEFAULT_EASE_DISTANCE 36

#define PARKING_MODE_NONE	0
#define PARKING_MODE_PARK	1
#define PARKING_MODE_DODO	2

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"
// .............................................................................Pins 


const char 	servo_pin[] 			= {32,33,25,26,27,14};
const char  note_pin[]				= {22,21,23,19};
const char	PIN_PIXELS				= 13;
const char 	NUM_SERVOS				= 6;
const char	NUM_NOTES				= 4;
const char	NUM_PIXELS				= 6;
const char	PIN_STATUS_PIX			= 1;
const char 	PIN_V_SENS				= 35;

//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
Preferences                             preferences;
ServoEasing 							myservo[NUM_SERVOS];
Timer									t;

CRGB									statusled[1];
CRGB                                    pixels[NUM_PIXELS];
CHSV									colors[NUM_PIXELS];

String 									hostname;

int										dmx_address;

float	servo_position[NUM_SERVOS];

unsigned char servo_detach;
unsigned  parking_mode;

unsigned int dmx_detach[DMX_CHANNELS];

unsigned long last_packet;

unsigned int debounce_time;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

int battery_max_ad, battery_min_ad;
int battery_low_ad;
int battery_monitor_interval;
long battery_last_check;

boolean isConnected;
boolean leds_changed;

boolean	settings_changed;
boolean		servo_channels_messed_up;
unsigned char servo_ease[NUM_SERVOS];
unsigned char servo_ease_distance[NUM_SERVOS];
unsigned char servo_channel[NUM_SERVOS];
unsigned char servo_speed[NUM_SERVOS];
unsigned char servo_startup[NUM_SERVOS];
unsigned char servo_minimum[NUM_SERVOS];
unsigned char servo_maximum[NUM_SERVOS];
unsigned char servo_detach_minimum[NUM_SERVOS];
unsigned char servo_detach_maximum[NUM_SERVOS];

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

void park(boolean detach) {
	for (int i = 0; i< NUM_SERVOS; i++) {
		if (!myservo[i].attached()) myservo[i].attach(servo_pin[i]);
		if (servo_ease[i] < 4) set_easing(i, 4);
		myservo[i].startEaseTo(servo_startup[i], servo_speed[i] / 2);
	}
	
	if (detach)		parking_mode = PARKING_MODE_DODO;
	else 			parking_mode = PARKING_MODE_PARK;
}

void detach_all() {
	for (int i = 0; i< NUM_SERVOS; i++) {
		myservo[i].detach();
	}
}

void attach_all() {
	for (int i = 0; i< NUM_SERVOS; i++) {
		myservo[i].attach(servo_pin[i]);
	}
}

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

void send_version() {
	const int send_version_reply_length = 21;
	uint8_t packet[send_version_reply_length];
	
//	Serial.print("Send: ");
	
	packet[0] = 0x80;  // header
	packet[1] = 0x80;  // timestamp, not implemented 
	packet[2] = 0xF0;  // SYSEX
	packet[3] = 0x7D;  // Homebrew Device
	
	packet[4] = SYSEX_VERSION_DATA;
	packet[5] = 12;					//length
	
	for (int i = 0; i < 12; i++) {
		packet[6+i] = version[i];
	}
		
	packet[19] = 0x80; // fake checksum
	packet[20] = 0xF7; 	// end of sysex

   pCharacteristic->setValue(packet, send_version_reply_length); // packet, length in bytes)
   pCharacteristic->notify();
}


void send_dmx_address() {
	const int reply_length = 10;
	uint8_t packet[reply_length];
	
	Serial.println("Send DMX Address");
	
	packet[0] = 0x80;  // header
	packet[1] = 0x80;  // timestamp, not implemented 
	packet[2] = 0xF0;  // SYSEX
	packet[3] = 0x7D;  // Homebrew Device
	
	packet[4] = SYSEX_DMX_ADDRESS;
	packet[5] = 2;					//length
	
	packet[6] = (dmx_address >> 7) & 0x7F;
	packet[7] = dmx_address & 0x7F;
		
	packet[8] = 0x80; // fake checksum
	packet[9] = 0xF7; 	// end of sysex

   pCharacteristic->setValue(packet, reply_length); // packet, length in bytes)
   pCharacteristic->notify();
}


void send_debounce_time() {
	const int reply_length = 10;
	uint8_t packet[reply_length];
	
	Serial.println("Send Debounce Time");
	
	packet[0] = 0x80;  // header
	packet[1] = 0x80;  // timestamp, not implemented 
	packet[2] = 0xF0;  // SYSEX
	packet[3] = 0x7D;  // Homebrew Device
	
	packet[4] = SYSEX_DEBOUNCE;
	packet[5] = 2;					//length
	
	packet[6] = (debounce_time >> 7) & 0x7F;
	packet[7] = debounce_time & 0x7F;
		
	packet[8] = 0x80; // fake checksum
	packet[9] = 0xF7; 	// end of sysex

   pCharacteristic->setValue(packet, reply_length); // packet, length in bytes)
   pCharacteristic->notify();
}


void get_param(int channel, int param){
//	Serial.print("Get Param ");	Serial.print(param);	Serial.print(" for channel ");Serial.println(channel);
	int return_val;
	switch (param) {
		case  PARAM_min : 
			return_val = servo_minimum[channel];
			break;
		case  PARAM_max : 
			return_val = servo_maximum[channel];
			break;
		case  PARAM_init : 
			return_val = servo_startup[channel];
			break;
		case  PARAM_detach_lo : 
			return_val = servo_detach_minimum[channel];
			break;
		case  PARAM_detach_hi : 
			return_val = servo_detach_maximum[channel];
			break;
		case  PARAM_ease_func : 
			return_val = servo_ease[channel];
			break;
		case  PARAM_speed : 
			return_val = servo_speed[channel];
			break;
		case  PARAM_ease_distance : 
			return_val = servo_ease_distance[channel];
			break;
		case  PARAM_channel : 
			return_val = servo_channel[channel] + 1;
			break;
		case PARAM_battery:
			switch (channel) {
				case 0:		
					return_val = battery_min_ad;
					break;
				case 1:		
					return_val = battery_max_ad;
					break;
				case 2:		
					return_val = battery_low_ad;
					break;
				case 3:		
					return_val = battery_monitor_interval;
					break;
			}
			break;
		default:
			return;
	}

	const int reply_length = 12;
	uint8_t packet[reply_length];

	packet[0] = 0x80;  // header
	packet[1] = 0x80;  // timestamp, not implemented 
	packet[2] = 0xF0;  // SYSEX
	packet[3] = 0x7D;  // Homebrew Device
	
	packet[4] = SYSEX_PARAM_DATA;
	packet[5] = 4;					//length
	
	packet[6] = channel + 1;
	packet[7] = param;

	packet[8] = (return_val >> 7) & 0x7F;
	packet[9] = return_val & 0x7F;

	packet[10] = 0x80; // fake checksum
	packet[11] = 0xF7; 	// end of sysex

   pCharacteristic->setValue(packet, reply_length); // packet, length in bytes)
   pCharacteristic->notify();

}

void set_param(int channel,int param, int value) {
	//Serial.print("Set Param ");	Serial.print(param);	Serial.print(" for channel ");Serial.print(channel);Serial.print(" to value ");Serial.println(value);

	switch (param) {
		case  PARAM_min : 
			if (value > 180) value = 180;
			servo_minimum[channel] = value;
			break;
		case  PARAM_max : 
			if (value > 180) value = 180;
			servo_maximum[channel] = value;
			break;
		case  PARAM_init : 
			if (value > 180) value = 180;
			servo_startup[channel] = value;
			dmx_detach[channel] = DMX_DETACH_TIME;
			myservo[channel].write(value);
			break;
		case  PARAM_detach_lo : 
			if (value > 127) value = 127;
			servo_detach_minimum[channel] = value;
			break;
		case  PARAM_detach_hi : 
			if (value > 127) value = 127;
			servo_detach_maximum[channel] = value;
			break;
		case  PARAM_ease_func :
			if (value > 4) value = 4;
			servo_ease[channel] = value;
			set_easing(channel,value);
			break;
		case  PARAM_speed : 
			if (value > 255) value = 255;
			servo_speed[channel] = value;
			break;
		case  PARAM_ease_distance : 
			if (value > 100) value = 100;
			if (value > 5) value = 5;
			servo_ease_distance[channel] = value;
			break;
		case  PARAM_channel : 
			if (value > 6) value = 6;
			if (value < 1) value = 1;
			servo_channel[channel] = value - 1;
			servo_channels_messed_up = false;
			for (int i = 0; i < NUM_SERVOS; i++) {
				if (servo_channel[i] != i ) servo_channels_messed_up = true;
			}
			if (servo_channels_messed_up) Serial.println("Messed up Servos");

			break;
			
		case PARAM_reset_all:
			if (value == 1 ) park(0);
			if (value == 2 ) attach_all();
			if (value == 3 ) detach_all();
			if (value == 4 ) park(1);
			if (value == 20 ) generate_default_values();
			break;
		case PARAM_battery:
			switch (channel) {
				case 0:		
					battery_min_ad = value;
					break;
				case 1:		
					battery_max_ad = value;
					break;
				case 2:		
					battery_low_ad = value;
					break;
				case 3:		
					battery_monitor_interval = value;
					break;
			}

	}
	settings_changed = true;
}


void check_settings_changed() {
	if 	(settings_changed) write_settings();
	settings_changed = false;
}

void set_easing(char chan, char val) {
	
	switch (val) {
		case 1:
			myservo[chan].setEasingType(EASE_LINEAR);
			break;
		case 2:
			myservo[chan].setEasingType(EASE_QUADRATIC_IN_OUT);
			break;
		case 3:
			myservo[chan].setEasingType(EASE_CUBIC_IN_OUT);
			break;
		case 4:
			myservo[chan].setEasingType(EASE_QUARTIC_IN_OUT);
			break;
		default:
			break;
	}
	
}

void led_control(char idx, char val) {
	if (idx < 7 ) return;
	if (idx == 7) { 					// channel 8:			global hue
		for (int i = 0; i < NUM_PIXELS; i++) {
			colors[i].hue = 2 * val;
			leds_changed = true;
		}
	} else if (idx == 8) { 					// channel 9:			global saturation
		for (int i = 0; i < NUM_PIXELS; i++) {
			colors[i].saturation = 2 * val;
			leds_changed = true;
		}
	} else if (idx == 9) { 					//channel 10:  			global brightness
		for (int i = 0; i < NUM_PIXELS; i++) {
			colors[i].value = 2 * val;
		}					
		leds_changed = true;
	} else {								//channels 11-16:		individual brightness
		if ((idx - 10) < NUM_PIXELS) {
			colors[(idx - 10)].value = 2 * val;
			leds_changed = true;
		} 
	}
}


void set_servo (char idx, char val) {
	val = map(val ,0,127,servo_minimum[idx],servo_maximum[idx]);
	if (servo_ease[idx] == 0) {
			myservo[idx].write(val);
	} else {
		if (abs(val-myservo[idx].getCurrentAngle()) < servo_ease_distance[idx]) {
			if (!myservo[idx].isMoving()) myservo[idx].write(val);
		} else {
			if (!myservo[idx].isMoving()) myservo[idx].startEaseTo(val,servo_speed[idx]);
		}
	}
}

void servo_control(char chan, char val){
	if (servo_channels_messed_up) {
		for (int i = 0; i < NUM_SERVOS; i++) {
			if (servo_channel[i] == chan) {
				set_servo(i,val);
			}
		}
	} else set_servo(chan,val);

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
		unsigned int temp ;
      if (rxValue.length() > 0) {
	      last_packet = millis();
//------------------------------						// Control Change
		if ((rxValue[2] >> 4 ) == 0x0B) {				
			int idx = rxValue[3]-1;
			char val = rxValue[4];
			if (val > 127) val = 127;
			
			if (idx < DMX_CHANNELS && idx >= 0) dmx_detach[idx] = DMX_DETACH_TIME;

			if (idx >= 0) {
				if (idx < NUM_SERVOS) {					// channels 1 - 6:	 	control servos
					//servo_val_raw[idx] = val;
					servo_control(idx,val);
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
			if (idx >= 10 && idx < 16) {
				dmx_detach[idx] = DMX_DETACH_TIME;
				led_control(idx,val);
			}
		} 
//------------------------------						// Note OFF
		if ((rxValue[2] >> 4 ) == 0x08) {				
			int idx = rxValue[3]-1;
			char val = rxValue[4];
			if (val > 127) val = 127;
			if (idx >= 10 && idx < 16) {
				dmx_detach[idx] = DMX_DETACH_TIME;
				led_control(idx,0);
			}
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
						Serial.println("Attempt name change");
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
					case SYSEX_SET_MINIMUM_HERE:
						if (channel < NUM_SERVOS) {
							servo_minimum[channel] = myservo[channel].read();
							set_limits(channel);
						}
						break;
					case SYSEX_SET_MAXIMUM_HERE:
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
					case SYSEX_GET_VERSION:
						send_version();
						break;
					case SYSEX_GET_DMX_ADDRESS:
						send_dmx_address();
						break;
					case SYSEX_GET_DEBOUNCE:
						send_debounce_time();
						break;
					case SYSEX_SET_DMX_ADDRESS:
						dmx_address = rxValue[6] << 7;
						dmx_address |= rxValue[7];
						if (dmx_address > 490) dmx_address = 490;
						Serial.print("DMX Address: ");
						Serial.println(dmx_address);
						
						preferences.begin("changlier", false);
						preferences.putInt("dmx_address",dmx_address);
						preferences.end();

						send_dmx_address();
						break;
					case SYSEX_SET_DEBOUNCE:
						debounce_time = rxValue[6] << 7;
						debounce_time |= rxValue[7];

						Serial.print("Debounce Time: ");
						Serial.println(debounce_time);
						
						preferences.begin("changlier", false);
						preferences.putInt("debounce_time",debounce_time);
						preferences.end();

						send_debounce_time();
						break;
					case SYSEX_GET_PARAM:
						get_param(channel,rxValue[7]);
						break;
					case SYSEX_SET_PARAM:
						set_param(channel,rxValue[7], (rxValue[8] << 7) | rxValue[9]);
						break;
					default:
						Serial.print("Strange Sysex: ");
						Serial.println(command);
						break;
					}
				}
			}
 	   }
    }
};


void print_settings() {
		Serial.println("======================================");
		Serial.println("CHANGLIER by Steve Octane Trio");
		Serial.println("======================================");
		Serial.print("Device Name: ");
		Serial.println(hostname);
		Serial.print("DMX Address: ");
		Serial.println(dmx_address);
		Serial.println("--------------");
		Serial.println("Settings");
		
	for (int i = 0; i < NUM_SERVOS; i++) {
		Serial.print("Servo ");
		Serial.print(i + 1);
		Serial.print(" MIN ");
		Serial.print(servo_minimum[i]);
		Serial.print(" MAX ");
		Serial.print(servo_maximum[i]);
		Serial.print(" Detach ");
		Serial.print(servo_detach_minimum[i]);
		Serial.print(" - ");
		Serial.print(servo_detach_maximum[i]);
		Serial.print(" INIT ");
		Serial.print(servo_startup[i]);
		Serial.print(" smooth ");
		Serial.print(servo_ease[i]);
		Serial.println();
	}
		Serial.println("--------------");
}
//----------------------------------------------------------------------------------------
//																				Preferences

void generate_default_values() {
	Serial.println(F("Generating default servo settings"));
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_minimum[i] = DEFAULT_MINIMUM;
    		servo_maximum[i] = DEFAULT_MAXIMUM;
   			servo_detach_minimum[i] = DEFAULT_MINIMUM_DETACH;
    		servo_detach_maximum[i] = DEFAULT_MINIMUM_DETACH;
    		servo_startup[i] = DEFAULT_INIT;
    		servo_ease[i] = DEFAULT_EASE;
    		servo_speed[i] = DEFAULT_SPEED;
    		servo_ease_distance[i] = DEFAULT_EASE_DISTANCE;
			servo_channel[i] = i;
    	}
    write_settings();
}  	
    	
    	
void read_preferences() {
    preferences.begin("changlier", false);
	
    hostname = preferences.getString("hostname");
    if (hostname == String()) { hostname = "Bebe Changlier"; }

	dmx_address = preferences.getInt("dmx_address",1);
	debounce_time = preferences.getInt("debounce_time",50);

	battery_max_ad = preferences.getInt("battery_max_ad",2048);
	battery_min_ad = preferences.getInt("battery_min_ad",1024);
	battery_low_ad = preferences.getInt("battery_low_ad",1600);
	battery_monitor_interval  = preferences.getInt("battery_monitor_interval",200);

   if(preferences.getBytesLength("minima") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_minimum[i] = DEFAULT_MINIMUM;
        	}
    	settings_changed = true;
    } else {
      	preferences.getBytes("minima",servo_minimum,NUM_SERVOS);
    }

   if(preferences.getBytesLength("maxima") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_maximum[i] = DEFAULT_MAXIMUM;
    	}
    	settings_changed = true;
    } else {
    	preferences.getBytes("maxima",servo_maximum,NUM_SERVOS);
    }
    
   if(preferences.getBytesLength("detach_minima") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
     		servo_detach_minimum[i] = DEFAULT_MINIMUM_DETACH;
    	}
    	settings_changed = true;
    } else {
        preferences.getBytes("detach_minima",servo_detach_minimum,NUM_SERVOS);
    }

   if(preferences.getBytesLength("detach_maxima") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
     		servo_detach_maximum[i] = DEFAULT_MINIMUM_DETACH;
    	}
    	settings_changed = true;
    } else {
    	preferences.getBytes("detach_maxima",servo_detach_maximum,NUM_SERVOS);
    }

   if(preferences.getBytesLength("startup") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_startup[i] = DEFAULT_INIT;
    	}
    	settings_changed = true;
    } else {
    	preferences.getBytes("startup",servo_startup,NUM_SERVOS);
    }

   if(preferences.getBytesLength("smooth") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_ease[i] = DEFAULT_EASE;
    	}
    	settings_changed = true;
    } else {
    	preferences.getBytes("smooth",servo_ease,NUM_SERVOS);
    }

   if(preferences.getBytesLength("speed") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
     		servo_speed[i] = DEFAULT_SPEED;
    	}
    	settings_changed = true;
    } else {
    	preferences.getBytes("speed",servo_speed,NUM_SERVOS);
    }
 
   if(preferences.getBytesLength("ease_distance") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
    		servo_ease_distance[i] = DEFAULT_EASE_DISTANCE;
    	}
    	settings_changed = true;
    } else {
    	preferences.getBytes("ease_distance",servo_ease_distance,NUM_SERVOS);
    }
   if(preferences.getBytesLength("channel") != NUM_SERVOS) {
    	for (int i = 0; i < NUM_SERVOS; i++) {
			servo_channel[i] = i;
    	}
    	settings_changed = true;
    } else {
    	preferences.getBytes("channel",servo_channel,NUM_SERVOS);
    }

	servo_channels_messed_up = false;
	for (int i = 0; i < NUM_SERVOS; i++) {
		if (servo_channel[i] != i ) servo_channels_messed_up = true;
	}
	if (servo_channels_messed_up) Serial.println("Messed up Servos");

	preferences.end();
	
	print_settings();
}

void write_settings() {
	preferences.begin("changlier", false);
    	preferences.putBytes("minima",servo_minimum,NUM_SERVOS);
    	preferences.putBytes("maxima",servo_maximum,NUM_SERVOS);
	   	preferences.putBytes("detach_minima",servo_detach_minimum,NUM_SERVOS);
    	preferences.putBytes("detach_maxima",servo_detach_maximum,NUM_SERVOS);
    	preferences.putBytes("startup",servo_startup,NUM_SERVOS);
    	preferences.putBytes("smooth",servo_ease,NUM_SERVOS);
    	preferences.putBytes("speed",servo_speed,NUM_SERVOS);
    	preferences.putBytes("ease_distance",servo_ease_distance,NUM_SERVOS);
    	preferences.putBytes("channel",servo_channel,NUM_SERVOS);
    	
		preferences.putInt("battery_max_ad",battery_max_ad);
		preferences.putInt("battery_min_ad",battery_min_ad);
		preferences.putInt("battery_low_ad",battery_low_ad);
		preferences.putInt("battery_monitor_interval",battery_monitor_interval);

	preferences.end();
	Serial.println("Settings written");
}
//----------------------------------------------------------------------------------------
//																				NOTES

void check_buttons() {
	static char old_button[NUM_NOTES];
	static int debounce[NUM_NOTES];
	
	for (int i = 0; i < NUM_NOTES; i++) {
		if (debounce[i]) {
			debounce[i]--;
		} else {
			char button = digitalRead(note_pin[i]);
			if (old_button[i] != button) {
				old_button[i] = button;
				debounce[i] = debounce_time;
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
}

//----------------------------------------------------------------------------------------
//																				LEDS

void update_leds() {
	if (!leds_changed) return;
	for (int i = 0; i < NUM_PIXELS; i++) {
		pixels[i] = colors[i];
	}
	FastLED.show();
	leds_changed = false;
}

//----------------------------------------------------------------------------------------
//																				SERVOS

void service_servos(){
	if (parking_mode == PARKING_MODE_NONE) {
		for (int i = 0; i < NUM_SERVOS; i++) {
			if (servo_ease[i]) {
				myservo[i].update();
			}
		}
	} else {
		boolean finished_parking = true;
		for (int i = 0; i < NUM_SERVOS; i++) {
			myservo[i].update();
			if (myservo[i].isMoving()) finished_parking = false;
		}

		if (finished_parking) {
			for (int i = 0; i< NUM_SERVOS; i++) {	
				set_easing(i, servo_ease[i]);
				dmx_detach[i] = DMX_DETACH_TIME;
			}
		
			if (parking_mode == PARKING_MODE_DODO) detach_all();
			parking_mode = PARKING_MODE_NONE;
		}
	}
}

//----------------------------------------------------------------------------------------
//																				DMX

// to prevent 'empty' DMX stream from moving all servos to their extremes, dmx is shifted up by one:
// DMX value 0:		 	-> NOP
// DMX value 1-128: 	-> MIDI 0-127
// DMX value > 128:		-> NOP

void check_dmx() {
	static char old_values[DMX_CHANNELS];
	char val , idx;
	
    if(DMX::IsHealthy()) {
		last_packet = millis();	    
		for (int i = 0; i < NUM_SERVOS; i++) {
			if (!dmx_detach[i]) {
				val = DMX::Read(i + dmx_address);
				if (val < 129 && val > 0) servo_control(i ,val - 1);
			}
		}
/*		val = DMX::Read(6 + dmx_address);
		if (val > 127) val = 127;
		servo_detach = val;
*/		
		for (int i = 7; i < 16; i++) {
			if (!dmx_detach[i]) {
				val = DMX::Read(i + dmx_address);
				if (val < 129 && val > 0) {
					if (val != old_values[i]) {
						old_values[i] = val;
						led_control(i,val - 1);
					}
				}
			}
		}
	} 
}

//----------------------------------------------------------------------------------------
//																				DMX detach when MIDI arrives

void check_dmx_detach(void) {
	for (int i = 0; i < DMX_CHANNELS; i++) {
		if (dmx_detach[i]) dmx_detach[i]--;
	}
}


//----------------------------------------------------------------------------------------
//																				Check Battery voltage

void check_battery(void) {
	int battery_state = analogRead(PIN_V_SENS);
	battery_state = map (battery_state,battery_min_ad,battery_max_ad,1,127);
	if (battery_state > 127) battery_state = 127;
	if (battery_state < 0) battery_state = 0;
	
	midiPacket[2] 	= 0xB0; 
	midiPacket[3] 	= 16;
	midiPacket[4] 	= battery_state;    // velocity
	
	pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
	pCharacteristic->notify();

	battery_last_check = millis();
}

//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup(){
    Serial.begin(115200);

    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN,HIGH);
	
	
	DMX::Initialize();

	Serial.println("Startup");
	FastLED.addLeds<SK6812, PIN_PIXELS, RGB>(pixels, NUM_PIXELS);
//	FastLED.addLeds<SK6812, PIN_STATUS_PIX, RGB>(statusled, 1);

	for (int hue = 0; hue < 360; hue++) {
    	fill_rainbow( pixels, NUM_PIXELS, hue, 7);
	//	statusled[0].setHSV(hue,127,127);
	    delay(3);
    	FastLED.show(); 
  	}
  
	for (int i = 0; i< NUM_PIXELS; i++) {
		colors[i].hue = 42; // yellow
		colors[i].saturation = 160;
		colors[i].value = 0;
	}
	leds_changed = true;


	for (int i = 0; i< NUM_NOTES; i++) {
		pinMode(note_pin[i], INPUT_PULLUP);
	}

	read_preferences();
	
	for (int i = 0; i< NUM_SERVOS; i++) {
		myservo[i].attach(servo_pin[i]);
		myservo[i].write(servo_startup[i]);
		set_easing(i, servo_ease[i]);
	}

 
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

  // Wait for servo to reach start position.
    delay(500);

	t.every(10,service_servos);
	t.every(10,check_dmx);
	t.every(1,check_buttons);
	t.every(20,update_leds);
	t.every(1000,check_dmx_detach);
	t.every(30000,check_settings_changed);
	digitalWrite(LED_BUILTIN,LOW);
	Serial.println("Setup finished");
}



//========================================================================================
//----------------------------------------------------------------------------------------
//																				loop
 
void loop(){
	t.update();
	
    if ((millis() - last_packet) > 200) {digitalWrite(LED_BUILTIN,LOW);}
    else {digitalWrite(LED_BUILTIN,HIGH); }

	if (battery_monitor_interval) {
	    if ((millis() - battery_last_check) > battery_monitor_interval) { check_battery(); }
	}

}
