//----------------------------------------------------------------------------------------
//
//	CHANGLIER Firmware
//						
//		Target MCU: DOIT ESP32 DEVKIT V1
//		Copyright:	2020 Michael Egger, me@anyma.ch
//		License: 	This is FREE software (as in free speech, not necessarily free beer)
//					published under gnu GPL v.3
//
//	  
//----------------------------------------------------------------------------------------

#ifndef __CHANCLIER_SYSEX
#define __CHANCLIER_SYSEX 1

#include "Changlier.h"


#define SYSEX_NOP				0

#define SYSEX_DATA_DUMP			1
#define SYSEX_DATA_DUMP_DATA	2


#define SYSEX_HW_VERSION		7
#define SYSEX_GET_HW_VERSION	8
#define SYSEX_SET_HW_VERSION	9

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

#define SYSEX_SET_SSID		80
#define SYSEX_SET_PASSWORD		81

#define SYSEX_GET_PARAM		90
#define SYSEX_PARAM_DATA	91
#define SYSEX_SET_PARAM		92

#define SYSEX_START_WIFI	100

#define SYSEX_CUSTOM		126



#define PARAM_min 				2
#define PARAM_max   3
#define PARAM_init   4
#define PARAM_detach_lo   5
#define PARAM_detach_hi   6
#define PARAM_ease_func   7
#define PARAM_speed   8
#define PARAM_ease_distance   9
#define PARAM_channel   10
#define PARAM_reset_all   11
#define PARAM_battery   12


void handle_sysex_builtin(std::string rxValue);
void send_sysex(char command, char * data, int len);
void send_servo_data(int channel);
void send_dmx_address();
void send_debounce_time();
void send_hw_version();

void get_param(int channel, int param);

void set_param(int channel,int param, int value);
void check_settings_changed();
void set_easing(char , char);
 
 
#endif