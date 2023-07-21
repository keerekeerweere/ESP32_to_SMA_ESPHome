#ifndef ESP32_SMA_H
#define ESP32_SMA_H

#include <ESP32Time.h>
#include "EspMQTTClient.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Update.h>

#include "site_details.h"
#include "ESP32_SMA_Inverter.h"
#include "ESP32Loggable.h"

//missing builtin led 
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif


//SMA inverter timezone (note inverter appears ignores summer time saving internally)
//Need to determine what happens when its a NEGATIVE time zone !
//Number of seconds for timezone
//    0=UTC (London)
//19800=5.5hours Chennai, Kolkata
//36000=Brisbane (UTC+10hrs)
#define timeZoneOffset (long)(60 * 60 * TIME_ZONE)


// max spot dc watt value, ignore if higher
#define MAX_SPOTDC 2000000000

//Do we switch off upload to sites when its dark?
#undef allowsleep
//#define allowsleep

class ESP32_SMA : public ESP32Loggable {
    public:
        ESP32_SMA() : ESP32Loggable("ESP32_SMA") {}

        void onConnectionEstablished();
        void setup();
        void loop();

        void everySecond();
        void every5Minutes();
        void everyHour();
        void everyDay();
        void dodelay(unsigned long duration);

// Function Prototypes
        void blinkLed();
        void blinkLedOff();
        //void printLocalTime();

    private:
        bool blinklaststate;

        EspMQTTClient mqttclient = EspMQTTClient(
            WIFI_SSID,
            WIFI_PASSWORD,
            MQTT_SERVER,
            MQTT_USER, // Can be omitted if not needed
            MQTT_PASS, // Can be omitted if not needed
            HOST);


        ESP32_SMA_Inverter smaInverter = ESP32_SMA_Inverter(mqttclient);

        ESP32Time ESP32rtc;     // Time structure. Holds what time the ESP32 thinks it is.
        ESP32Time nextMidnight; // Create a time structure to hold the answer to "What time (in time_t seconds) is the upcoming midnight?"

        // "datetime" stores the number of seconds since the epoch (normally 01/01/1970), AS RETRIEVED
        //     from the SMA. The value is updated when data is read from the SMA, like when
        //     getInstantACPower() is called.
        //static unsigned long datetime=0;   // stores the number of seconds since the epoch (normally 01/01/1970)
        time_t datetime = 0;
        unsigned long sleepuntil = 0;
        unsigned long nextSecond = 0;
        unsigned long next5Minute = 0;
        unsigned long nextHour = 0;
        unsigned long nextDay = 0;
        int thisminute = -1;
        int checkbtminute = -1;


        const unsigned long seventy_years = 2208988800UL;


};









#endif
