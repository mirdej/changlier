#include "Changlier.h"
#include "ChanglierSYSEX.h"
#include "ChanglierOTA.h"
#include "ChanglierBLE.h"

//----------------------------------------------------------------------------------------
//																				Handle SYSEX

void handle_sysex_builtin(std::string rxValue) {
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
		case SYSEX_SET_SSID:
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
				preferences.begin("changlier", false);
				preferences.putString("ssid",new_name);
				preferences.end();
			}
			break;
		case SYSEX_SET_PASSWORD:
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
				preferences.begin("changlier", false);
				preferences.putString("password",new_name);
				preferences.end();
			}
			break;
		case SYSEX_START_WIFI:
			enable_wifi();
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
			send_status_back = 1;
			send_sysex(SYSEX_VERSION_DATA, (char *)version, 12);
			break;
		case SYSEX_GET_DMX_ADDRESS:
			send_dmx_address();
			break;
		case SYSEX_GET_DEBOUNCE:
			send_debounce_time();
			break;
		case SYSEX_SET_HW_VERSION:
			hardware_version = rxValue[6] << 7;
			hardware_version |= rxValue[7];

			Serial.print("Hardware Version: ");
			Serial.println(hardware_version);
			preferences.begin("changlier", false);
			preferences.putInt("hw",hardware_version);
			preferences.end();
			break;
		case SYSEX_GET_HW_VERSION:
			send_hw_version();
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
		case SYSEX_CUSTOM:
			handle_sysex(rxValue.substr(6, rxValue.length()-9 ));
		default:
			break;
	}
}

//----------------------------------------------------------------------------------------
//																				Send SYSEX


void send_sysex(char command, char * data, int len) {
	int packet_length = len + 8;
	char packet[packet_length];
	
	Serial.print("Send Sysex ");	
	
	packet[0] = 0x80;  // header
	packet[1] = 0x80;  // timestamp, not implemented 
	packet[2] = 0xF0;  // SYSEX
	packet[3] = 0x7D;  // Homebrew Device
	packet[4] = command;
	packet[5] = len;
	
	for (int i = 0; i < len; i++) {
		packet[6 + i] = data[i];
	}
	
	packet[len + 6] = 0x80; // fake checksum
	packet[len + 7] = 0xF7; 	// end of sysex
	
	for (int i = 0; i < packet_length; i++) {		
		Serial.print(packet[i],DEC);
		Serial.print(" ");
	}
	Serial.println(); 

	pCharacteristic->setValue((uint8_t *)packet, packet_length); // packet, length in bytes)
	pCharacteristic->notify();
}



//----------------------------------------------------------------------------------------
//																				Servo Data

void send_servo_data(int channel) {
	char packet[7];
	
	packet[0] = channel;
	packet[1] = servo_minimum[channel] >> 1;
	packet[2] = servo_minimum[channel] & 1;
	packet[3] = myservo[channel].getCurrentAngle() >> 1;
	packet[4] = myservo[channel].getCurrentAngle() & 1;
	packet[5] = servo_maximum[channel] >> 1;
	packet[6] = servo_maximum[channel] & 1;
	send_sysex(SYSEX_SERVODATA, packet, 7);
}

//----------------------------------------------------------------------------------------
//																				DMX Address

void send_dmx_address() {
	char packet[2];
	packet[0] = (dmx_address >> 7) & 0x7F;
	packet[1] = dmx_address & 0x7F;
	send_sysex(SYSEX_DMX_ADDRESS, packet, 2);
}

void send_hw_version() {
	char packet[2];
	packet[0] = (hardware_version >> 7) & 0x7F;
	packet[1] = hardware_version & 0x7F;
	send_sysex(SYSEX_HW_VERSION, packet, 2);
}

//----------------------------------------------------------------------------------------
//																				Debounce

void send_debounce_time() {
	char packet[2];
	packet[0] = (debounce_time >> 7) & 0x7F;
	packet[1] = debounce_time & 0x7F;
	send_sysex(SYSEX_DEBOUNCE, packet, 2);
}

//----------------------------------------------------------------------------------------
//																				Get Param

void get_param(int channel, int param){

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

	const int reply_length = 4;
	char packet[reply_length];

	packet[0] = channel + 1;
	packet[1] = param;

	packet[2] = (return_val >> 7) & 0x7F;
	packet[3] = return_val & 0x7F;
	
	send_sysex(SYSEX_PARAM_DATA, packet, reply_length);
}

//----------------------------------------------------------------------------------------
//																				Set Param


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


