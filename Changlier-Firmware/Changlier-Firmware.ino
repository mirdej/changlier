const char * version = "2020-04-11.1";

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

#include "Changlier.h"
#include "ChanglierSYSEX.h"
#include "ChanglierOTA.h"
#include "ChanglierBLE.h"

// .............................................................................Pins 


const char 	servo_pin[] 			= {32,33,25,26,27,14};
const char  note_pin[]				= {22,21,23,19};
const char	PIN_PIXELS				= 13;
const char	PIN_ENABLE_SERVOS1_4	= 15;
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

int	hardware_version;

CRGB									statusled[1];
CRGB                                    pixels[NUM_PIXELS];
CHSV									colors[NUM_PIXELS];

String 									hostname;

int										dmx_address;

float	servo_position[NUM_SERVOS];

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


//----------------------------------------------------------------------------------------
//																				MIDI Callbacks

void handle_control_change(char ctl, char val) {
	char idx = ctl - 1;
	if (val > 127)

	if (idx < DMX_CHANNELS && idx >= 0) dmx_detach[idx] = DMX_DETACH_TIME;

	if (idx >= 0) {
		if (idx < NUM_SERVOS) {					// channels 1 - 6:	 	control servos
			//servo_val_raw[idx] = val;
			servo_control(idx,val);
		} else if (idx == 6) { 
			if (hardware_version >= HARDWARE_VERSION_20200303_VD) {
				if (val > 64) {
					digitalWrite(PIN_ENABLE_SERVOS1_4,HIGH);
				} else {
					digitalWrite(PIN_ENABLE_SERVOS1_4,LOW);
				}	
			}				
		} else {
			led_control(idx,val);
		}
	}

}


void handle_note_on(char note, char velocity) {
	int idx = note - 1;
	char val = velocity;
	if (val > 127) val = 127;
	if (idx >= 10 && idx < 16) {
		dmx_detach[idx] = DMX_DETACH_TIME;
		led_control(idx,val);
	}
}



void handle_note_off(char note, char velocity) {
	int idx = note - 1;
	char val = velocity;
	if (val > 127) val = 127;
	if (idx >= 10 && idx < 16) {
		dmx_detach[idx] = DMX_DETACH_TIME;
		led_control(idx,0);
	}
}

//----------------------------------------------------------------------------------------
//																				Control


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

//----------------------------------------------------------------------------------------
//																				Enable Wifi for OTA updates




//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup(){
    Serial.begin(115200);

    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN,HIGH);
	
	DMX::Initialize();

	Serial.println("Startup");

	read_preferences();
	
	if (hardware_version >= HARDWARE_VERSION_20200303_VD) {
		pinMode(PIN_ENABLE_SERVOS1_4, OUTPUT);
		digitalWrite(PIN_ENABLE_SERVOS1_4,LOW);
	}
	
	for (int i = 0; i< NUM_SERVOS; i++) {
		myservo[i].attach(servo_pin[i]);
		myservo[i].write(servo_startup[i]);
		set_easing(i, servo_ease[i]);
	}
	
	bluetooth_init() ;

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
	wifi_enabled = false;

	for (int i = 0; i< NUM_NOTES; i++) {
		pinMode(note_pin[i], INPUT_PULLUP);
	}
		
	// Wait for servo to reach start position.
    delay(1000);

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
	if (wifi_enabled)  {server.handleClient(); delay(1); }
	else {
		 t.update();
		 		
		if ((millis() - last_packet) > 200) {digitalWrite(LED_BUILTIN,LOW);}
		else {digitalWrite(LED_BUILTIN,HIGH); }

		if (hardware_version >=  HARDWARE_VERSION_20200303_V) {
			if (battery_monitor_interval) {
				if ((millis() - battery_last_check) > battery_monitor_interval) { check_battery(); }
			}
		}
	}
}
