#include "ChanglierBLE.h"
#include "ChanglierSYSEX.h"
#include "ChanglierOTA.h"

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
						send_version();
						break;
					case SYSEX_GET_DMX_ADDRESS:
						send_dmx_address();
						break;
					case SYSEX_GET_DEBOUNCE:
						send_debounce_time();
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
					default:
						Serial.print("Strange Sysex: ");
						Serial.println(command, DEC);
						break;
					}
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