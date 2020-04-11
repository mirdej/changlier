#include "Changlier.h"
#include "ChanglierSYSEX.h"
#include "ChanglierOTA.h"
#include "ChanglierBLE.h"

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

	hardware_version = HARDWARE_VERSION_20200303_VD;

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