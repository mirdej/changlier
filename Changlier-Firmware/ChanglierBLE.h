#ifndef __CHANGLIER_BLE
#define __CHANGLIER_BLE

#include "Changlier.h"
#include "ChanglierSYSEX.h"


extern BLECharacteristic *pCharacteristic;
extern bool deviceConnected;

extern  uint8_t midiPacket[];
void bluetooth_init();

void send_midi_note_on(char note, char velocity);
void send_midi_note_off(char note, char velocity);
void send_midi_control_change(char ctl, char val);

#endif