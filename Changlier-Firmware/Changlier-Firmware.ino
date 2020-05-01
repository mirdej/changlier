char  version[16];

//----------------------------------------------------------------------------------------
//
//	CHANGLIER Firmware
//						
//		Target MCU: DOIT ESP32 DEVKIT V1
//		Variant: Changlier, Patrition Scheme:  Minimal SPIFFS (1.9M APP with OTA, 190kb SPIFFS)
//		Copyright:	2019 Michael Egger, me@anyma.ch
//		License: 	This is FREE software (as in free speech, not necessarily free beer)
//					published under gnu GPL v.3
//
//		DMX Library: https://github.com/luksal/ESP32-DMX-RX
//		Attention buffer overflow bug on line 134
//
//----------------------------------------------------------------------------------------

#include "Changlier.h"

// .............................................................................Pins 


const char 	servo_pin[] 			= {32,33,25,26,27,14};
const char  note_pin[]				= {22,21,23,19};
const char	PIN_PIXELS				= 13;
const char	PIN_ENABLE_SERVOS1_4	= 15;
const char	PIN_ENABLE_SERVOS5_6	= 4;
const char 	PIN_V_SENS				= 35;


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
			servo_control(idx,val);
		} else if (idx == 6) {
			detach_control(val);
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

void handle_sysex(std::string data) {
	for (int i=0; i< data.length(); i++) {
		Serial.print(data[i]);
	}
	Serial.println(); 
	
	if (data == "Hallo") {
		char reply[] = "Hello";
		send_sysex(116, reply, 5);
	}
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
				if (button) {
					send_midi_note_off( 5+i, 0);
					if (hostname == "Manu") {
						led_control(i+10,0);
					}
					if (hostname == "ines") {
						led_control(i+10,0);
					}
				} else {
					send_midi_note_on( 5+i, 127);
					if (hostname == "Manu") {
						led_control(i+10,127);
					}
					if (hostname == "ines") {
						led_control(i+10,127);
					}
				}
			}
		}
	}
}

void makeversion(char const *date,char const *time, char *buff) { 
    int month, day, year,hour,min,sec;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    sscanf(date, "%s %d %d", buff, &day, &year);
    month = (strstr(month_names, buff)-month_names)/3+1;
	sscanf(time, "%d:%d:%d", &hour, &min, &sec);
    sprintf(buff, "%d.%02d%02d.%02d", year, month, day, hour);
}

//========================================================================================
//----------------------------------------------------------------------------------------
//																				SETUP

void setup(){
    Serial.begin(115200);

	makeversion(__DATE__, __TIME__, version);

  
    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN,HIGH);
	
	DMX::Initialize();

	Serial.println("Startup");

	read_preferences();

	
	if (hardware_version >= HARDWARE_VERSION_20200303_VD) {
		pinMode(PIN_ENABLE_SERVOS1_4, OUTPUT);
		pinMode(PIN_ENABLE_SERVOS5_6, OUTPUT);
		digitalWrite(PIN_ENABLE_SERVOS1_4,LOW);
		digitalWrite(PIN_ENABLE_SERVOS5_6,LOW);
	}
	
	for (int i = 0; i< NUM_SERVOS; i++) {
		myservo[i].attach(servo_pin[i]);
		myservo[i].write(servo_startup[i]);
		set_easing(i, servo_ease[i]);
	}
	
	bluetooth_init() ;

	FastLED.addLeds<SK6812, PIN_PIXELS, RGB>(pixels, NUM_PIXELS);

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
	leds_changed = true;
	wifi_enabled = false;

	for (int i = 0; i< NUM_NOTES; i++) {
		pinMode(note_pin[i], INPUT_PULLUP);
	}
		
	// Wait for servo to reach start position.
    delay(1000);

	t.every(1,check_buttons);	
	t.every(10,service_servos);
	t.every(10,check_dmx);
	t.every(50,update_leds);
	t.every(50,live_update);
	t.every(100,check_battery);
	t.every(1000,check_dmx_detach);
	t.every(10000,check_settings_changed);
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
	}
}
