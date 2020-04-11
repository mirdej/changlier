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


void send_servo_data(int channel);
void send_version();
void send_dmx_address();
void send_debounce_time();

void get_param(int channel, int param);

void set_param(int channel,int param, int value);
void check_settings_changed();
void set_easing(char , char);
 
 
#endif