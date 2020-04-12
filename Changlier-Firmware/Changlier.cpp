#include "Changlier.h"
#include "ChanglierSYSEX.h"
#include "ChanglierOTA.h"
#include "ChanglierBLE.h"


//========================================================================================
//----------------------------------------------------------------------------------------
//																	GLOBALS
Preferences                 preferences;
ServoEasing 				myservo[NUM_SERVOS];
Timer						t;

int							hardware_version;

CRGB                        pixels[NUM_PIXELS];
CHSV						colors[NUM_PIXELS];

String 						hostname;

int							dmx_address;

unsigned  					parking_mode;

unsigned int 				dmx_detach[DMX_CHANNELS];

unsigned long 				last_packet;

unsigned int 				debounce_time;


int 						battery_max_ad, battery_min_ad;
int							battery_low_ad;
int 						battery_monitor_interval;
long 						battery_last_check;

boolean 					isConnected;
boolean 					leds_changed;

boolean						settings_changed;
boolean						servo_channels_messed_up;
unsigned char 				servo_ease[NUM_SERVOS];
unsigned char 				servo_ease_distance[NUM_SERVOS];
unsigned char 				servo_channel[NUM_SERVOS];
unsigned char 				servo_speed[NUM_SERVOS];
unsigned char 				servo_startup[NUM_SERVOS];
unsigned char 				servo_minimum[NUM_SERVOS];
unsigned char 				servo_maximum[NUM_SERVOS];
unsigned char 				servo_detach_minimum[NUM_SERVOS];
unsigned char 				servo_detach_maximum[NUM_SERVOS];

//========================================================================================
//----------------------------------------------------------------------------------------
//																				IMPLEMENTATION




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
	wifi_enabled = preferences.getInt("wifi",1);
	
	preferences.putString("ssid","Anymair");
	preferences.putString("password","Mot de passe pas complique");
    ssid = preferences.getString("ssid");
    password = preferences.getString("password");
    
    
     
	debounce_time = preferences.getInt("debounce_time",50);

	hardware_version = preferences.getInt("hardware_version",HARDWARE_VERSION_20200303);

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
//																				LEDS

void update_leds() {

	if ((millis() - last_packet) > 200) {digitalWrite(LED_BUILTIN,LOW);}
	else {digitalWrite(LED_BUILTIN,HIGH); }

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
		if (hardware_version >=  HARDWARE_VERSION_20200303_V) {
			if (battery_monitor_interval) {
				if ((millis() - battery_last_check) > battery_monitor_interval) { 
					int battery_state = analogRead(PIN_V_SENS);
					battery_state = map (battery_state,battery_min_ad,battery_max_ad,1,127);
					if (battery_state > 127) battery_state = 127;
					if (battery_state < 0) battery_state = 0;
	
					send_midi_control_change( 16 , battery_state);

					battery_last_check = millis();
				}
			}
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
