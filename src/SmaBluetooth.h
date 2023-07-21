#ifndef SMA_BLUETOOTH_H
#define SMA_BLUETOOTH_H

#include "Arduino.h"
#include <time.h>
#include "BluetoothSerial.h"
#include <EEPROM.h>
#include <esp_bt_device.h> // ESP32 BLE
#include <logging.hpp>
#include "Utils_SMA.h"
#include "site_details.h"
#include "ESP32Loggable.h"


#define INVERTERSCAN_PIN 14 //Analogue pin 1 - next to VIN on connectors
#define BT_KEY 15           //Forces BT BOARD/CHIP into AT command mode
#define RxD 16
#define TxD 17
#define BLUETOOTH_POWER_PIN 5 //pin 5

//Location in EEPROM where the 2 arrays are written
#define ADDRESS_MY_BTADDRESS 0
#define ADDRESS_SMAINVERTER_BTADDRESS 10

#define STATE_FRESH 0
#define STATE_SETUP 1
#define STATE_CONNECTED 2




class ESP32SmaBluetooth : public ESP32Loggable {
    public:
        ESP32SmaBluetooth() : ESP32Loggable("ESP32SmaBluetooth") {}
        bool BTStart();
        bool BTCheckConnected();
        String getDeviceAddress(const uint8_t *point);
        void updateMyDeviceAddress();
        //void sendPacket(unsigned char *btbuffer);
        void sendPacket(unsigned char *btbuffer, unsigned int pposition );
        void writeArrayIntoEEPROM(unsigned char readbuffer[], int length, int EEPROMoffset);
        bool readArrayFromEEPROM(unsigned char readbuffer[], int length, int EEPROMoffset);


        unsigned char getByte();
        void convertBTADDRStringToArray(char *tempbuf, unsigned char *outarray, char match);

    private: 
        uint8_t address[6] = {SMA_ADDRESS};
        uint8_t btstate = STATE_FRESH;
        bool connected;
        //static long charTime = 0;

        unsigned char myBTAddress[6] = {}; // BT address of ESP32.

        BluetoothSerial SerialBT;


    friend class ESP32_SMANetArduino;
};


int hex2bin(const char *s);

#endif