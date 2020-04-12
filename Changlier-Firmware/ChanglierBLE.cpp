#include "ChanglierBLE.h"
#include "ChanglierSYSEX.h"
#include "ChanglierOTA.h"



BLECharacteristic *pCharacteristic;
bool deviceConnected;

uint8_t midiPacket[] = {
   0x80,  // header
   0x80,  // timestamp, not implemented 
   0x00,  // status
   0x3c,  // 0x3c == 60 == middle c
   0x00   // velocity
};



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
				handle_control_change(rxValue[3], rxValue[4]);
			} 
		
	//------------------------------						// Note ON
			if ((rxValue[2] >> 4 ) == 0x09) {				
				handle_note_on(rxValue[3], rxValue[4]);
			} 
	//------------------------------						// Note OFF
			if ((rxValue[2] >> 4 ) == 0x08) {				
				handle_note_off(rxValue[3], rxValue[4]);
			} 

	//------------------------------					// SYSEX	
			if (rxValue[2] == 0xF0) {					
				if (rxValue[3] == 0x7D) {				// Homebrew Device
					handle_sysex_builtin(rxValue);
				}
			}				
		}
	}
};


void bluetooth_init() {
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
}



//----------------------------------------------------------------------------------------
//																				Note ON

void send_midi_note_on(char note, char velocity) {
		
	midiPacket[2] = 0x90; // note on, channel 0
	midiPacket[3] 	= note;
	midiPacket[4] = velocity;
						
	pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
	pCharacteristic->notify();
}

//----------------------------------------------------------------------------------------
//																				Note OFF


void send_midi_note_off(char note, char velocity){
		
	midiPacket[2] = 0x80; // note off, channel 0
	midiPacket[3] 	= note;
	midiPacket[4] = velocity;    
						
	pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
	pCharacteristic->notify();
}

//----------------------------------------------------------------------------------------
//																				Control Change


void send_midi_control_change(char ctl, char val){
	midiPacket[2] 	= 0xB0; 
	midiPacket[3] 	= ctl;
	midiPacket[4] 	= val;    // velocity
	
	pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
	pCharacteristic->notify();
}
