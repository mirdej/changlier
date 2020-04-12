#ifndef __CHANGLIER_H
#define __CHANGLIER_H 1
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


#include <esp32-dmx-rx.h>

#include <Preferences.h>
#include "ServoEasing.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Timer.h>
#include <FastLED.h>

#include "ChanglierSYSEX.h"
#include "ChanglierOTA.h"
#include "ChanglierBLE.h"


//========================================================================================
//----------------------------------------------------------------------------------------
//																				DEFINES

#define NUM_SERVOS				6
#define	NUM_NOTES				4
#define	NUM_PIXELS				6

#define	DMX_DETACH_TIME			10
#define	DMX_CHANNELS			20


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

#define	HARDWARE_VERSION_UNKNOWN		 0
#define	HARDWARE_VERSION_2				 2			// june 2019 with display
#define	HARDWARE_VERSION_20200303		 3			// eurocircuits, matte finish, vanilla
#define	HARDWARE_VERSION_20200303_V		 4			// eurocircuits, matte finish, with voltage sensor mod
#define	HARDWARE_VERSION_20200303_VD	 5			// eurocircuits, matte finish, with voltage sensor mod + detach on 74HC244


#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"



// .............................................................................Pins 


extern const char 	servo_pin[] ;
extern const char  	note_pin[];	
extern const char	PIN_PIXELS;
extern const char	PIN_ENABLE_SERVOS1_4;
extern const char	PIN_STATUS_PIX;
extern const char 	PIN_V_SENS;

//========================================================================================
//----------------------------------------------------------------------------------------
//																				GLOBALS
extern Preferences                             preferences;
extern ServoEasing 							myservo[];
extern Timer									t;

extern int	hardware_version;

extern CRGB                                    pixels[];
extern CHSV									colors[];

extern String 									hostname;

extern int										dmx_address;

extern float	servo_position[];

extern unsigned  parking_mode;

extern unsigned int dmx_detach[];

extern unsigned long last_packet;

extern unsigned int debounce_time;

extern int battery_max_ad, battery_min_ad;
extern int battery_low_ad;
extern int battery_monitor_interval;
extern long battery_last_check;

extern boolean isConnected;
extern boolean leds_changed;

extern boolean	settings_changed;
extern boolean		servo_channels_messed_up;
extern unsigned char servo_ease[];
extern unsigned char servo_ease_distance[];
extern unsigned char servo_channel[];
extern unsigned char servo_speed[];
extern unsigned char servo_startup[];
extern unsigned char servo_minimum[];
extern unsigned char servo_maximum[];
extern unsigned char servo_detach_minimum[];
extern unsigned char servo_detach_maximum[];

extern uint8_t midiPacket[];
extern const char * version;
//========================================================================================
//----------------------------------------------------------------------------------------
//																				prototypes

void handle_control_change(char ctl, char val);
void handle_note_on(char note, char velocity);
void handle_note_off(char note, char velocity);
void handle_sysex(std::string rxValue);

void park(boolean detach);
void detach_all();
void attach_all();
void led_control(char idx, char val);
void set_servo (char idx, char val);
void servo_control(char chan, char val);
void set_limits(char channel);
void print_settings();
void generate_default_values();
void read_preferences();
void write_settings();
void check_buttons();
void update_leds();
void service_servos();
void check_dmx();
void check_dmx_detach(void);
void check_battery(void);

#endif