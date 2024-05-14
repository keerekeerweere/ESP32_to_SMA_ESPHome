/*
 Talks to SMA Sunny Boy, Model SB 8000US-12
    - Gets AC power (only)
    - Sends the result to a custom LED bargraph

  Based on code found at https://github.com/stuartpittaway/nanodesmapvmonitor
 */

//Need to change SoftwareSerial/NewSoftSerial.h file to set buffer to 128 bytes or you will get buffer overruns!
//Find the line below and change
//#define _NewSS_MAX_RX_BUFF 128 // RX buffer size

#include "Arduino.h"
#include "ESP32_SMA.h"

#include "time.h"
#include "bluetooth.h"
#include "LocalUtil.h"
#include "mainstate.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Update.h>
#include "site_details.h"

#include "EspMQTTClient.h"

#include <logging.hpp>
#include <ets-appender.hpp>
#include <udp-appender.hpp>
#include "ESP32_SMA_Inverter.h"


using namespace esp32m;


#ifdef SLEEP_ROUTINE
ESP32Time wakeupTime;
ESP32Time bedTime;
#endif
//#define WAKEUP_TIME 22,30 
//#define BED_TIME 05,45

#ifdef SYSLOG_HOST
#ifdef SYSLOG_PORT
  UDPAppender udpappender(SYSLOG_HOST, SYSLOG_PORT);
#else
  UDPAppender udpappender(SYSLOG_HOST, 514);
#endif
#endif


//BST Start and end dates - this needs moving into some sort of PROGMEM array for the years or calculated based on the BST logic see
//http://www.time.org.uk/bstgmtcodepage1.aspx
//static time_t SummerStart=1435888800;  //Sunday, 31 March 02:00:00 GMT
//static time_t SummerEnd=1425952800;  //Sunday, 27 October 02:00:00 GMT


size_t xArraySize(void);


void ESP32_SMA::blinkLed()
{
  digitalWrite(LED_BUILTIN, blinklaststate);
  blinklaststate = !blinklaststate;
}

void ESP32_SMA::blinkLedOff()
{
  if (blinklaststate)
  {
    blinkLed();
  }
}


ESP32_SMA esp32sma ;

void onConnectionEstablished() {
  esp32sma.onConnectionEstablished();
}

void ESP32_SMA::onConnectionEstablished()
{
  mqttclient.publish(MQTT_BASE_TOPIC "LWT", "online", true);
  logW("WiFi and MQTT connected");
  logW("v1 Build 2w d (%s) t (%s) "  ,__DATE__ , __TIME__) ;

#ifdef PUBLISH_HASS_TOPICS
  // client.publish(MQTT_BASE_TOPIC "LWT", "online", true); 
  mqttclient.publish("homeassistant/sensor/" HOST "/signal/config", "{\"name\": \"Signal Strength\", \"device_class\": \"signal_strength\", \"state_topic\": \"" MQTT_BASE_TOPIC "signal\", \"unique_id\": \"" HOST "-signal\", \"unit_of_measurement\": \"dB\", \"entity_category\": \"diagnostic\", \"device\": {\"identifiers\": [\"" HOST "-device\"], \"name\": \"" FRIENDLY_NAME "\"}}", true);
  mqttclient.publish("homeassistant/sensor/" HOST "/generation_today/config", "{\"name\": \" Power Generation Today\", \"device_class\": \"energy\", \"state_topic\": \"" MQTT_BASE_TOPIC "generation_today\", \"unique_id\": \"" HOST "-generation_today\", \"unit_of_measurement\": \"kWh\", \"state_class\": \"total_increasing\", \"device\": {\"identifiers\": [\"" HOST "-device\"]} }", true);
  mqttclient.publish("homeassistant/sensor/" HOST "/generation_total/config", "{\"name\": \" Power Generation Total\", \"device_class\": \"energy\", \"state_topic\": \"" MQTT_BASE_TOPIC "generation_total\", \"unique_id\": \"" HOST "-generation_total\", \"unit_of_measurement\": \"kWh\", \"state_class\": \"total_increasing\", \"device\": {\"identifiers\": [\"" HOST "-device\"]} }", true);
  mqttclient.publish("homeassistant/sensor/" HOST "/instant_ac/config", "{\"name\": \" Instantinous AC Power\", \"device_class\": \"power\", \"state_topic\": \"" MQTT_BASE_TOPIC "instant_ac\", \"unique_id\": \"" HOST "-instant_ac\", \"unit_of_measurement\": \"W\", \"state_class\": \"measurement\", \"device\": {\"identifiers\": [\"" HOST "-device\"]} }", true);
  mqttclient.publish("homeassistant/sensor/" HOST "/instant_dc/config", "{\"name\": \" Instantinous DC Power\", \"device_class\": \"power\", \"state_topic\": \"" MQTT_BASE_TOPIC "instant_dc\", \"unique_id\": \"" HOST "-instant_dc\", \"unit_of_measurement\": \"W\", \"state_class\": \"measurement\", \"device\": {\"identifiers\": [\"" HOST "-device\"]} }", true);
  mqttclient.publish("homeassistant/sensor/" HOST "/instant_vdc/config", "{\"name\": \" Instantinous DC Voltage\", \"device_class\": \"voltage\", \"state_topic\": \"" MQTT_BASE_TOPIC "instant_vdc\", \"unique_id\": \"" HOST "-instant_vdc\", \"unit_of_measurement\": \"V\", \"state_class\": \"measurement\", \"device\": {\"identifiers\": [\"" HOST "-device\"]} }", true);
  mqttclient.publish("homeassistant/sensor/" HOST "/instant_adc/config", "{\"name\": \" Instantinous DC Ampere\", \"device_class\": \"current\", \"state_topic\": \"" MQTT_BASE_TOPIC "instant_adc\", \"unique_id\": \"" HOST "-instant_adc\", \"unit_of_measurement\": \"A\", \"state_class\": \"measurement\", \"device\": {\"identifiers\": [\"" HOST "-device\"]} }", true);

#endif // PUBLISH_HASS_TOPICS
}

void setup() {
  esp32sma.setup();
}

void ESP32_SMA::setup()
{
  Serial.begin(115200);                      //Serial port for debugging output

  Logging::setLevel(esp32m::Info);
  Logging::addAppender(&ETSAppender::instance());
#ifdef SYSLOG_HOST
  udpappender.setMode(UDPAppender::Format::Syslog);
  Logging::addAppender(&udpappender);
#endif
  Serial.println("added appenders");

  // Get the MAC address in reverse order (not sure why, but do it here to make setup easier)
  unsigned char smaSetAddress[6] = {SMA_ADDRESS};
  for (int i = 0; i < 6; i++)
    smaBTInverterAddressArray[i] = smaSetAddress[5 - i];

  pinMode(LED_BUILTIN, OUTPUT);

  ESP32rtc.setTime(30, 24, 15, 17, 1, 2021); // 17th Jan 2021 15:24:30  // Need this to be accurate. Since connecting to the internet anyway, use NTP.

  mqttclient.setMaxPacketSize(512); // must be big enough to send home assistant config
  mqttclient.enableMQTTPersistence();
  mqttclient.enableDebuggingMessages();                                     // Enable debugging messages sent to serial output
  mqttclient.enableHTTPWebUpdater("/update");                               // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
  mqttclient.enableLastWillMessage(MQTT_BASE_TOPIC "LWT", "offline", true); // You can activate the retain flag by setting the third parameter to true

  logW("Connected to %s" , WIFI_SSID);
  logW("IP address: %s" , WiFi.localIP().toString().c_str());

  // Always set time to GMT timezone
  configTime(timeZoneOffset, 0, NTP_SERVER);
  logW("2W Connected to %s" , WIFI_SSID);
  
  Logging::hookUartLogger();
//  setupOTAServer();


  logD("hello world!");
}

void ESP32_SMA::everySecond()
{
  blinkLedOff();
}

void ESP32_SMA::every5Minutes()
{
  log_d("every5Minutes()");
  if (mqttclient.isConnected())
  {
    mqttclient.publish(MQTT_BASE_TOPIC "signal", String(WiFi.RSSI()));
  }
}

void ESP32_SMA::everyHour()
{

}

void ESP32_SMA::everyDay()
{

}


void ESP32_SMA::dodelay(unsigned long duration)
{
  sleepuntil = millis() + duration;
}

//uint8_t mainstate = MAINSTATE_INIT;
MainState mainstate = MAINSTATE_INIT;

static long lastRanTime = 0;
static long nowTime = 0;

void loop() {
  esp32sma.loop();
}


void ESP32_SMA::loop()
{
  struct tm timeinfo;
  mqttclient.loop();
  // server.handleClient();
  
  if (millis() >= nextSecond)
  {
    nextSecond = millis() + 1000;
    everySecond();
  }

  if (millis() >= next5Minute)
  {
    next5Minute = millis() + 1000 * 60 * 5;
    every5Minutes();
  }

  if (millis() >= nextHour)
  {
    nextHour = millis() + 1000 * 3600;
    everyHour();
  }

  if (millis() >= nextDay)
  {
    nextDay = millis() + 1000 * 3600 * 24 ;
    everyDay();
  }

  // "delay" the main BT loop
  if (millis() < sleepuntil)
    return;

  // if in the main loop, only run at the top of the minute
  if (mainstate >= MAINSTATE_GET_INSTANT_AC_POWER)
  {
    getLocalTime(&timeinfo);
    if (timeinfo.tm_min == thisminute)
      return;

    
    // Check we are connected to BT, if not, restart process
    if (!BTCheckConnected())
    {
      mainstate = MAINSTATE_INIT;
    }
  }

  // Wait for initial NTP sync before setting up inverter
  if (mainstate == MAINSTATE_INIT && ESP32rtc.getEpoch() < AFTER_NOW)
  { configTime(timeZoneOffset, 0, NTP_SERVER); // just added configtime multiple times to make this work. Not a nice workaround...

    log_d("NTP not yet sync'd, sleeping");
    log_w("NTP not yes sync'd, sleeping 10s");
    dodelay(10000);
    configTime(timeZoneOffset, 0, NTP_SERVER);

  }

  // Only bother to do anything if we are connected to WiFi and MQTT
  if (!mqttclient.isConnected())
    return;

  // Run the "main" BT loop
  blinkLed();

  log_d("Loop mainstate: %s", mainstate.toString().c_str());

  switch (mainstate)
  {
  case MAINSTATE_INIT:
    if (BTStart())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    }
    else
    {
      dodelay(1000);
    }
    break;

  case MAINSTATE_INIT_SMA_CONNECTION:
    if (smaInverter.initialiseSMAConnection())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    }
    break;

  case MAINSTATE_LOGON_SMA_INVERTER:
    if (smaInverter.logonSMAInverter())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    }
    break;

  case MAINSTATE_GET_DAILY_YIELD_3 : //3
    // Doing this to set datetime
    if (smaInverter.getDailyYield())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    }
    break;

  case MAINSTATE_CHECK_SET_INVERTER_TIME: // 4:
    if (smaInverter.checkIfNeedToSetInverterTime())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    }
    break;

    // --------- regular loop -----------------

  case MAINSTATE_GET_INSTANT_AC_POWER:// 5:
    if (smaInverter.getInstantACPower())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    } 
    break;

  case MAINSTATE_GET_INSTANT_DC_POWER: //6:
    if (smaInverter.getInstantDCPower())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    } 
    break;

  case MAINSTATE_GET_DAILY_YIELD_7:// 7:
    if (smaInverter.getDailyYield())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    }
    break;

  case MAINSTATE_TOTAL_POWER_GENERATION: //8:
    if (smaInverter.getTotalPowerGeneration())
    {
      mainstate++;
      smaInverter.resetInnerstate();
    }
    break;

  default:
    mainstate = MAINSTATE_GET_INSTANT_AC_POWER;
    smaInverter.resetInnerstate();
    thisminute = timeinfo.tm_min;
  }
  return;

  //getInverterName();
  //HistoricData();
  lastRanTime = millis() - 4000;
  log_i("While...");

  while (1)
  {

    //HowMuchMemory();

    //Populate datetime and spotpowerac variables with latest values
    //getInstantDCPower();
    nowTime = millis();
    if (nowTime > (lastRanTime + 4000))
    {
      lastRanTime = nowTime;
      Serial.print("^");
      smaInverter.getInstantACPower();
    }

    //digitalClockDisplay(now());

    //The inverter always runs in UTC time (and so does this program!), and ignores summer time, so fix that here...
    //add 1 hour to readings if its summer
    // DRH Temp removal of: if ((datetime>=SummerStart) && (datetime<=SummerEnd)) datetime+=60*60;

#ifdef SLEEP_ROUTINE

 //wakeupTime;
 //bedTime;
#endif



#ifdef allowsleep
    if ((ESP32rtc.getEpoch() > (datetime + 3600)) && (spotpowerac == 0))
    {
      //Inverter output hasnt changed for an hour, so put Nanode to sleep till the morning
      //debugMsgln("Bed time");

      //sleeping=true;

      //Get midnight on the day of last solar generation
      // First, create a time structure to hold the answer to "At what time (in time_t seconds) is the upcoming midnight?"
      tmElements_t tm;
      tm.Year = year(datetime) - 1970;
      tm.Month = month(datetime);
      tm.Day = day(datetime);
      tm.Hour = 23;
      tm.Hour = hour(datetime);
      tm.Minute = 59;
      tm.Second = 59;
      time_t midnight = makeTime(tm);

      //Move to midnight
      //debugMsg("Midnight ");digitalClockDisplay( midnight );debugMsgln("");

      if (ESP32rtc.getEpoch() < midnight)
      {
        //Time to calculate SLEEP time, we only do this if its BEFORE midnight
        //on the day of solar generation, otherwise we might wake up and go back to sleep immediately!

        //Workout what time sunrise is and sleep till then...
        //longitude, latitude (london uk=-0.126236,51.500152)
        unsigned int minutespastmidnight = ComputeSun(mylongitude, mylatitude, datetime, true);

        //We want to wake up at least 15 minutes before sunrise, just in case...
        checktime = midnight + minutespastmidnight - 15 * 60;
      }
    }
#endif

    //digitalClockDisplay( checktime );

    //Delay for approx. 4 seconds between instant AC power readings

  } // end of while(1)
} // end of loop()
