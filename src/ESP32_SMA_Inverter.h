#ifndef ESP32_SMA_INVERTER_H
#define ESP32_SMA_INVERTER_H



#include "Arduino.h"
#include "SmaBluetooth.h"
#include "SMANetArduino.h"
#include "LocalUtil.h"
#include "mainstate.h"
#include "ESP32Time.h"
#include "EspMQTTClient.h"
//#include <logging.hpp>
#include "ESP32Loggable.h"

#include "site_details.h"


//using namespace esp32m;

//missing builtin led 
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif


//User Group
#define UG_USER         0x07L
#define UG_INSTALLER    0x0AL

//Wellknown SUSyID's
#define SID_MULTIGATE   175
#define SID_SB240       244


//SMA inverter timezone (note inverter appears ignores summer time saving internally)
//Need to determine what happens when its a NEGATIVE time zone !
//Number of seconds for timezone
//    0=UTC (London)
//19800=5.5hours Chennai, Kolkata
//36000=Brisbane (UTC+10hrs)
#define timeZoneOffset (long)(60 * 60 * TIME_ZONE)


// A time in Unix Epoch that is "now" - used to check that the NTP server has
// sync'd the time correctly before updating the inverter
#define AFTER_NOW  1630152740 // 2319321600

// max spot dc watt value, ignore if higher
#define MAX_SPOTDC 2000000000

#define INIT_smanet2packetx80x00x02x00 {0x80, 0x00, 0x02, 0x00}
//size_t sizeof_smanet2packetx80x00x02x00 = sizeof(INIT_smanet2packetx80x00x02x00);

#define INIT_smanet2packet2 {0x80, 0x0E, 0x01, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
//static size_t sizeof_smanet2packet2 = sizeof(smanet2packet2);

#define INIT_smanet2packet6 {0x54, 0x00, 0x22, 0x26, 0x00, 0xFF, 0x22, 0x26, 0x00}
//static size_t sizeof_smanet2packet6 = sizeof(smanet2packet6);

#define INIT_smanet2packet99 {0x00, 0x04, 0x70, 0x00}
//static size_t sizeof_smanet2packet99 = sizeof(smanet2packet99);

#define INIT_smanet2packet0x01000000 {0x01, 0x00, 0x00, 0x00}
//static size_t sizeof_smanet2packet0x01000000 = sizeof(smanet2packet0x01000000);

#define INIT_smanet2packet_logon {0x80, 0x0C, 0x04, 0xFD, 0xFF, 0x07, 0x00, 0x00, 0x00, 0x84, 0x03, 0x00, 0x00, 0xaa, 0xaa, 0xbb, 0xbb}

#define INIT_smanet2acspotvalues {0x51, 0x00, 0x3f, 0x26, 0x00, 0xFF, 0x3f, 0x26, 0x00, 0x0e}

#define INIT_smanet2packetdcpower { 0x83, 0x00, 0x02, 0x80, 0x53, 0x00, 0x00, 0x25, 0x00, 0xFF, 0xFF, 0x25, 0x00}

#define INIT_smanet2settime {0x8c, 0x0a, 0x02, 0x00, 0xf0, 0x00, 0x6d, 0x23, 0x00, 0x00, 0x6d, 0x23, 0x00, 0x00, 0x6d, 0x23, 0x00}

#define INIT_smanet2totalyieldWh { 0x54, 0x00, 0x01, 0x26, 0x00, 0xFF, 0x01, 0x26, 0x00}

#define INIT_fourzeros {0, 0, 0, 0}

//const unsigned char SMAInverterPasscode[] = {SMA_INVERTER_USER_PASSCODE};

class ESP32_SMA_Inverter : public ESP32Loggable {

    public:
        ESP32_SMA_Inverter(EspMQTTClient& client) :  ESP32Loggable("ESP32SmaInverter"),
            _client(client),
            smanet2packetx80x00x02x00 INIT_smanet2packetx80x00x02x00,
            smanet2packet2 INIT_smanet2packet2,
            smanet2packet6 INIT_smanet2packet6,
            smanet2packet99 INIT_smanet2packet99,
            smanet2packet0x01000000 INIT_smanet2packet0x01000000,
            smanet2packet_logon INIT_smanet2packet_logon,
            smanet2acspotvalues INIT_smanet2acspotvalues,
            smanet2packetdcpower INIT_smanet2packetdcpower,
            smanet2settime INIT_smanet2acspotvalues,
            smanet2totalyieldWh INIT_smanet2totalyieldWh,
            fourzeros INIT_fourzeros,
            SMAInverterPasscode SMA_INVERTER_USER_PASSCODE 
            {
            }

        bool initialiseSMAConnection();
        bool logonSMAInverter();
        bool logoffSMAInverter();
        bool getInstantACPower();
        bool getInstantDCPower();
        bool getPowerGeneration();
        bool checkIfNeedToSetInverterTime();
        void setInverterTime();
        void setInverterAddress(unsigned char* inverterAddress);

        void resetInnerstate() { innerstate = 0; };

        bool start();
        bool isConnected();

    protected:
        EspMQTTClient& _client;
        ESP32_SMANetArduino smanet;


    private:

        prog_uchar PROGMEM smanet2packetx80x00x02x00[4];
        prog_uchar PROGMEM smanet2packet2[9];
        prog_uchar PROGMEM smanet2packet6[9];
        prog_uchar PROGMEM smanet2packet99[4];
        prog_uchar PROGMEM smanet2packet0x01000000[4];
        prog_uchar PROGMEM smanet2packet_logon[17];
        prog_uchar PROGMEM smanet2acspotvalues[10];
        prog_uchar PROGMEM smanet2packetdcpower[13];
        prog_uchar PROGMEM smanet2settime[17];
        prog_uchar PROGMEM smanet2totalyieldWh[9];
        prog_uchar PROGMEM fourzeros[4];

        //sizeOf defines
        const size_t sizeof_smanet2packetx80x00x02x00 = sizeof(smanet2packetx80x00x02x00);
        const size_t sizeof_smanet2packet2 = sizeof(smanet2packet2);
        const size_t sizeof_smanet2packet99 = sizeof(smanet2packet99);
        const size_t sizeof_smanet2packet6 = sizeof(smanet2packet6);
        const size_t sizeof_smanet2packet0x01000000 = sizeof(smanet2packet0x01000000);
        const size_t sizeof_smanet2packet_logon = sizeof(smanet2packet_logon);
        const size_t sizeof_smanet2acspotvalues = sizeof(smanet2acspotvalues);
        const size_t sizeof_smanet2packetdcpower = sizeof(smanet2packetdcpower);
        const size_t sizeof_smanet2settime = sizeof(smanet2settime);
        const size_t sizeof_smanet2totalyieldWh = sizeof(smanet2totalyieldWh);
        const size_t sizeof_fourzeros = sizeof(fourzeros);

        ESP32Time esp32rtc;

        //static prog_uchar PROGMEM SMANET2header[] = {0xFF, 0x03, 0x60, 0x65};
        //static prog_uchar PROGMEM InverterCodeArray[] = {0x5c, 0xaf, 0xf0, 0x1d, 0x50, 0x00}; // Fake address on the SMA NETWORK
        //static prog_uchar PROGMEM fourzeros[] = {0, 0, 0, 0};

        //Password needs to be 12 bytes long, with zeros as trailing bytes (Assume SMA INVERTER PIN code is 0000)
        unsigned char SMAInverterPasscode[12];


        //function call innerstate variable
        uint8_t innerstate = 0;

        uint64_t currentvalue = 0;
        unsigned int valuetype = 0;
        unsigned long value = 0;
        uint64_t value64 = 0;

        long lastRanTime = 0;
        long nowTime = 0;

        time_t datetime = 0;

        unsigned long spotpowerac = 0;
        unsigned long spotpowerdc = 0;
        float spotvoltdc=0;
        float spotampdc=0;

        const unsigned short anySUSyID = 0xFFFF;
        const unsigned long anySerial = 0xFFFFFFFF;

        unsigned short devSUSyID;
        unsigned long devSerial;

    friend class ESP32_SMANetArduino;

};

#endif