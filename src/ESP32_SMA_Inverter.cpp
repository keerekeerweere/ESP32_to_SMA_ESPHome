#include "ESP32_SMA_Inverter.h"


#include <ets-appender.hpp>
#include <udp-appender.hpp>

#include "site_details.h"



void ESP32_SMA_Inverter::setInverterAddress(unsigned char* inverterAddress) {
  for (int i = 0; i < 6; i++)
    smanet.smaBTInverterAddressArray[i] = inverterAddress[5 - i];

}


bool ESP32_SMA_Inverter::initialiseSMAConnection()
{
  logD("initialiseSMAConnection(%i) ", innerstate);


  unsigned char netid;
  switch (innerstate)
  {
  case 0:
    //Wait for announcement/broadcast message from PV inverter
    if (smanet.getPacket(0x0002))
      innerstate++;
    break;

  case 1:
    //Extract data from the 0002 packet
    netid = smanet.level1packet[4];

    // Now create a response and send it.
    for (int i = 0; i < sizeof(smanet.level1packet); i++)
      smanet.level1packet[i] = 0x00;

    smanet.writePacketHeader(smanet.level1packet, 0x02, smanet.smaBTInverterAddressArray);
    smanet.writeSMANET2Long(smanet.level1packet, 0x00700400); //writeLong(pcktBuf, 0x00700400); // smanet.writeSMANET2ArrayFromProgmem(smanet.level1packet, smanet2packet99, sizeof_smanet2packet99 );
    smanet.writeSMANET2SingleByte(smanet.level1packet, netid); //writeByte(pcktBuf, invData->NetID);
    smanet.writeSMANET2Long(smanet.level1packet, 0); //writeLong(pcktBuf, 0); //smanet.writeSMANET2ArrayFromProgmem(smanet.level1packet, fourzeros, sizeof_fourzeros ); 
    smanet.writeSMANET2Long(smanet.level1packet, 1); //writeLong(pcktBuf, 1); //smanet.writeSMANET2ArrayFromProgmem(smanet.level1packet, smanet2packet0x01000000, sizeof(smanet2packet0x01000000));
    smanet.writePacketLength(smanet.level1packet);
    smanet.sendPacket(smanet.level1packet);
    innerstate++;
    break;

  case 2:
    // The SMA inverter will respond with a packet carrying the command '0x000A'.
    // It will return with cmdcode set to 0x000A.
    if (smanet.getPacket(0x000a))
      innerstate++;
    break;

  case 3:
    // The SMA inverter will now send two packets, one carrying the '0x000C' command, then the '0x0005' command.
    // Sit in the following loop until you get one of these two packets.
    smanet.cmdcode = smanet.readLevel1PacketFromBluetoothStream(0);
    if ((smanet.cmdcode == 0x000c) || (smanet.cmdcode == 0x0005))
      innerstate++;
    break;

  case 4:
    // If the most recent packet was command code = 0x0005 skip this next line, otherwise, wait for 0x0005 packet.
    // Since the first SMA packet after a 0x000A packet will be a 0x000C packet, you'll probably sit here waiting at least once.
    if (smanet.cmdcode == 0x0005)
    {
      innerstate++;
    }
    else
    {
      if (smanet.getPacket(0x0005))
        innerstate++;
    }
    break;

  case 5:
    //First SMANET2 packet
    smanet.writePacketHeader(smanet.level1packet, 0x01, smanet.address_unknown);
    smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x09, 0xA0, 0, anySUSyID, anySerial);//smanet.level1packet, 0x09, 0xa0, smanet.packet_send_counter, 0, 0, 0);

    smanet.writeSMANET2Long(smanet.level1packet, 0x00000200);
    smanet.writeSMANET2Long(smanet.level1packet, 0);
    smanet.writeSMANET2Long(smanet.level1packet, 0);

    smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet);
    smanet.writePacketLength(smanet.level1packet);

    smanet.sendPacket(smanet.level1packet);

    if (smanet.getPacket(0x0001) && smanet.validateChecksum())
    {
      innerstate++;
      smanet.packet_send_counter++;
    }

    devSUSyID = LocalUtil::get_short(smanet.level1packet + 55); //get_short(pcktBuf + 55);
    devSerial = LocalUtil::get_long(smanet.level1packet + 57); //Serial = get_long(pcktBuf + 57);
    
    logI("SUSyID: %d - SN: %lu\n", devSUSyID, devSerial);

    break;

  case 6:
    //Second SMANET2 packet
    logoffSMAInverter();
/*    smanet.writePacketHeader(smanet.level1packet, smanet.sixff);
    smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x08, 0xa0, smanet.packet_send_counter, 0x00, 0x03, 0x03);
    smanet.writeSMANET2ArrayFromProgmem(smanet.level1packet, smanet2packet2, sizeof_smanet2packet2);

    smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet);
    smanet.writePacketLength(smanet.level1packet);
    smanet.sendPacket(smanet.level1packet);
    smanet.packet_send_counter++;

    innerstate++;
    break;
*/
  default:
    return true;
  }

  return false;
}


/*
uint8_t  RootDeviceAddress[6]= {0, 0, 0, 0, 0, 0};    //Hold byte array with BT address of primary inverter
uint8_t  LocalBTAddress[6] = {0, 0, 0, 0, 0, 0};      //Hold byte array with BT address of local adapter
uint8_t  addr_broadcast[6] = {0, 0, 0, 0, 0, 0};
uint8_t  addr_unknown[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
const unsigned short AppSUSyID = 125;
unsigned long  AppSerial;
const unsigned short anySUSyID = 0xFFFF;
const unsigned long anySerial = 0xFFFFFFFF;
*/

bool ESP32_SMA_Inverter::logonSMAInverter()
{
  logD("logonSMAInverter(%i)" , innerstate);
  time_t now;

  //Third SMANET2 packet
  switch (innerstate)
  {
  case 0:
    smanet.writePacketHeader(smanet.level1packet, 0x01, smanet.address_unknown); // pass dest address unknown // writePacketHeader(pcktBuf, 0x01, addr_unknown);
    //smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x0e, 0xa0, smanet.packet_send_counter, 0x00, 0x01, 0x01);
    //smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x0e, 0xa0, 0x0100, smanet.packet_send_counter);
    //0x0E, 0xA0, 0x0100
    smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x0e, 0xa0, 0x0100, anySUSyID, anySerial); // writePacket(pcktBuf, 0x0E, 0xA0, 0x0100, anySUSyID, anySerial);

    //was before smanet.writeSMANET2ArrayFromProgmem(smanet.level1packet, smanet2packet_logon, sizeof(smanet2packet_logon));    
    smanet.writeSMANET2Long(smanet.level1packet, 0xFFFD040C); //writeLong(pcktBuf, 0xFFFD040C);
    smanet.writeSMANET2Long(smanet.level1packet, UG_USER);    // User / Installer
    smanet.writeSMANET2Long(smanet.level1packet, 0x00000384); // Timeout = 900sec ?
    now = time(NULL);
    smanet.writeSMANET2Long(smanet.level1packet, now);
    smanet.writeSMANET2Long(smanet.level1packet, 0); //was before smanet.writeSMANET2ArrayFromProgmem(smanet.level1packet, fourzeros, sizeof_fourzeros);

    //INVERTER PASSWORD
    for (int passcodeloop = 0; passcodeloop < sizeof(SMAInverterPasscode); passcodeloop++)
    {
      unsigned char v = pgm_read_byte(SMAInverterPasscode + passcodeloop);
      smanet.writeSMANET2SingleByte(smanet.level1packet, (v + 0x88) % 0xff);
    }

    smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet);
    smanet.writePacketLength(smanet.level1packet);
    smanet.sendPacket(smanet.level1packet);

    innerstate++;
    break;

  case 1:
    if (smanet.getPacket(0x0001) && smanet.validateChecksum())
    {
      innerstate++;
      smanet.packet_send_counter++;
    }
    break;

  default:
    return true;
  }

  return false;
}

bool ESP32_SMA_Inverter::logoffSMAInverter()
{
  //Third SMANET2 packet
    logD("logoffSMAInverter()");

    //pcktID++;
    smanet.writePacketHeader(smanet.level1packet, 0x01, smanet.address_unknown);
    smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x08, 0xA0, 0x0300, anySUSyID, anySerial);
    smanet.writeSMANET2Long(smanet.level1packet, 0xFFFD010E);
    smanet.writeSMANET2Long(smanet.level1packet, 0xFFFFFFFF);
    smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet);
    smanet.writePacketLength(smanet.level1packet);
    smanet.sendPacket(smanet.level1packet);

    innerstate++;

    return true;
}

/**
 case SpotACPower:
        // SPOT_PAC1, SPOT_PAC2, SPOT_PAC3
        command = 0x51000200;
        first = 0x00464000;
        last = 0x004642FF;
        break; 
*/
bool ESP32_SMA_Inverter::getInstantACPower()
{
  logD("getInstantACPower(%i)" , innerstate);

  long command = 0x51000200;
  long first = 0x00464000;
  long last = 0x004642FF;

  int32_t thisvalue;
  //Get spot value for instant AC wattage
  // debugMsg("getInstantACPower stage: ");
  // debugMsgLn(String(innerstate));

  switch (innerstate)
  {
  case 0:
    //writePacketHeader(pcktBuf, 0x01, addr_unknown);
    smanet.writePacketHeader(smanet.level1packet, smanet.smaBTInverterAddressArray); 
   
    // writePacket(pcktBuf, 0x09, 0xA0, 0, device->SUSyID, device->Serial);
    smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x09, 0xA0, 0, devSUSyID, devSerial);     

    smanet.writeSMANET2Long(smanet.level1packet, command);
    smanet.writeSMANET2Long(smanet.level1packet, first);
    smanet.writeSMANET2Long(smanet.level1packet, last);

    smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet); //writePacketTrailer(pcktBuf);
    smanet.writePacketLength(smanet.level1packet); //writePacketLength(pcktBuf);

    smanet.sendPacket(smanet.level1packet);
    innerstate++;
    break;

  case 1:
    if (smanet.waitForMultiPacket(0x0001))
    {
      if (smanet.validateChecksum())
      {
        smanet.packet_send_counter++;
        innerstate++;
      }
      else
        innerstate = 0;
    }
    break;

  case 2:
    //value will contain instant/spot AC power generation along with date/time of reading...
    // memcpy(&datetime, &level1packet[40 + 1 + 4], 4);
    bool validPcktID = false;

    short pcktcount = LocalUtil::get_short(smanet.level1packet + 25);
    unsigned short rcvpcktID = LocalUtil::get_short(smanet.level1packet + 27) & 0x7FFF;

    //was it my packet ? 
    uint16_t aSYSyID = LocalUtil::get_short(smanet.level1packet + 15);
    uint32_t aSerial = LocalUtil::get_long(smanet.level1packet+17);
    if (((uint16_t) aSYSyID == devSUSyID) && ((uint32_t)aSerial == devSerial)) 
    {
        validPcktID = true;
        int32_t value = 0;
        int64_t value64 = 0;
        uint32_t recordsize = 4 * ((uint32_t)smanet.level1packet[5] - 9) / ((uint32_t)LocalUtil::get_long(smanet.level1packet + 37) - (uint32_t)LocalUtil::get_long(smanet.level1packet + 33) + 1);
        for (int ii = 41; ii < smanet.packetposition - 3; ii += recordsize) 
        {
          uint8_t *recptr = smanet.level1packet + ii;
          uint32_t code = ((uint32_t)LocalUtil::get_long(recptr));
          LriDef lri = (LriDef)(code & 0x00FFFF00);
          uint32_t cls = code & 0xFF;
          uint8_t dataType = code >> 24;
          time_t datetime = (time_t)LocalUtil::get_long(recptr + 4);

          if (recordsize == 16)
          {
              value64 = LocalUtil::get_longlong(recptr + 8);
              if (is_NaN(value64) || is_NaN((uint64_t)value64))
                  value64 = 0;
          }
          else if ((dataType != DT_STRING) && (dataType != DT_STATUS))
          {
              value = LocalUtil::get_long(recptr + 16);
              if (is_NaN(value) || is_NaN((uint32_t)value))
                  value = 0;
          }

          logD("received valuetype :%%.%dX ", lri);

          switch (lri)
          {
          case GridMsWphsA: //SPOT_PACTOT
              //This function gives us the time when the inverter was switched off
              thisvalue = value;
              logI("AC Pwr= %li " , thisvalue);

              _client.publish(MQTT_BASE_TOPIC "instant_ac", LocalUtil::uint64ToString(currentvalue), true);

              spotpowerac = thisvalue;

              break;

          case GridMsWphsB: //SPOT_PACTOT
              //This function gives us the time when the inverter was switched off
              thisvalue = value;
              logI("AC Pwr= %li " , thisvalue);

              _client.publish(MQTT_BASE_TOPIC "instant_ac", LocalUtil::uint64ToString(currentvalue), true);

              spotpowerac = thisvalue;

              break;
          case GridMsWphsC: //SPOT_PACTOT
              //This function gives us the time when the inverter was switched off
              thisvalue = value;
              logI("AC Pwr= %li " , thisvalue);

              _client.publish(MQTT_BASE_TOPIC "instant_ac", LocalUtil::uint64ToString(currentvalue), true);

              spotpowerac = thisvalue;

              break;
          default:
             break;
          }
        }
    }

    innerstate++;
    break;

    return true;
  }

  return false;
}

bool ESP32_SMA_Inverter::getPowerGeneration()
{
  //Gets the total kWh the SMA inverter has generated in its lifetime...
  // debugMsg("getTotalPowerGeneration stage: ");
  // debugMsgLn(String(innerstate));
  logD("getTotalPowerGeneration(%i)" , innerstate);
        // SPOT_ETODAY, SPOT_ETOTAL
        long command = 0x54000200;
        long first = 0x00260100;
        long last = 0x002622FF;

  switch (innerstate)
  {
  case 0:
    smanet.writePacketHeader(smanet.level1packet);
    //smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x09, 0xa0, smanet.packet_send_counter, 0, 0, 0);
    smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x09, 0xA0, 0, devSUSyID, devSerial);
    smanet.writeSMANET2Long(smanet.level1packet, command);
    smanet.writeSMANET2Long(smanet.level1packet, first);
    smanet.writeSMANET2Long(smanet.level1packet, last);

    smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet);
    smanet.writePacketLength(smanet.level1packet);

    smanet.sendPacket(smanet.level1packet);
    innerstate++;
    break;

  case 1:
    if (smanet.waitForMultiPacket(0x0001))
    {
      if (smanet.validateChecksum())
      {
        smanet.packet_send_counter++;
        innerstate++;
      }
      else
        innerstate = 0;
    }
    break;

  case 2:
    //displaySpotValues(16);

    bool validPcktID = false;

    short pcktcount = LocalUtil::get_short(smanet.level1packet + 25);
    unsigned short rcvpcktID = LocalUtil::get_short(smanet.level1packet + 27) & 0x7FFF;

    //was it my packet ? 
    uint16_t aSYSyID = LocalUtil::get_short(smanet.level1packet + 15);
    uint32_t aSerial = LocalUtil::get_long(smanet.level1packet+17);
    if (((uint16_t) aSYSyID == devSUSyID) && ((uint32_t)aSerial == devSerial)) 
    {
        validPcktID = true;
        int32_t value = 0;
        int64_t value64 = 0;
        uint32_t recordsize = 4 * ((uint32_t)smanet.level1packet[5] - 9) / ((uint32_t)LocalUtil::get_long(smanet.level1packet + 37) - (uint32_t)LocalUtil::get_long(smanet.level1packet + 33) + 1);
        for (int ii = 41; ii < smanet.packetposition - 3; ii += recordsize) 
        {
          uint8_t *recptr = smanet.level1packet + ii;
          uint32_t code = ((uint32_t)LocalUtil::get_long(recptr));
          LriDef lri = (LriDef)(code & 0x00FFFF00);
          uint32_t cls = code & 0xFF;
          uint8_t dataType = code >> 24;
          time_t datetime = (time_t)LocalUtil::get_long(recptr + 4);

          if (recordsize == 16)
          {
              value64 = LocalUtil::get_longlong(recptr + 8);
              if (is_NaN(value64) || is_NaN((uint64_t)value64))
                  value64 = 0;
          }
          else if ((dataType != DT_STRING) && (dataType != DT_STATUS))
          {
              value = LocalUtil::get_long(recptr + 16);
              if (is_NaN(value) || is_NaN((uint32_t)value))
                  value = 0;
          }

          logD("received valuetype :%%.%dX ", lri);

          switch (lri)
          {

          case MeteringTotWhOut: //SPOT_ETOTAL
              //In case SPOT_ETODAY missing, this function gives us inverter time (eg: SUNNY TRIPOWER 6.0)
              logI("SPOT_ETOTAL= %f" , (double)value64 / 1000);
              _client.publish(MQTT_BASE_TOPIC "generation_total", LocalUtil::uint64ToString(value64), true);
              break;

          case MeteringDyWhOut: //SPOT_ETODAY
              //This function gives us the current inverter time
              logI("SPOT_ETODAY= %f" , (double)value64 / 1000);
              _client.publish(MQTT_BASE_TOPIC "generation_today", LocalUtil::uint64ToString(value64), true);
              break;

          default:
             break;
          }
        }
    }

    innerstate++;
    break;

    return true;
  }

  return false;
}

bool ESP32_SMA_Inverter::getInstantDCPower()
{
  logD("getInstantACPower(%i)" , innerstate);

  long command = 0x51000200;
  long first = 0x00464000;
  long last = 0x004642FF;

  int32_t thisvalue;
  //Get spot value for instant AC wattage
  // debugMsg("getInstantACPower stage: ");
  // debugMsgLn(String(innerstate));

  switch (innerstate)
  {
  case 0:
    //writePacketHeader(pcktBuf, 0x01, addr_unknown);
    smanet.writePacketHeader(smanet.level1packet, smanet.smaBTInverterAddressArray); 
   
    // writePacket(pcktBuf, 0x09, 0xA0, 0, device->SUSyID, device->Serial);
    smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x09, 0xA0, 0, devSUSyID, devSerial);     

    smanet.writeSMANET2Long(smanet.level1packet, command);
    smanet.writeSMANET2Long(smanet.level1packet, first);
    smanet.writeSMANET2Long(smanet.level1packet, last);

    smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet); //writePacketTrailer(pcktBuf);
    smanet.writePacketLength(smanet.level1packet); //writePacketLength(pcktBuf);

    smanet.sendPacket(smanet.level1packet);
    innerstate++;
    break;

  case 1:
    if (smanet.waitForMultiPacket(0x0001))
    {
      if (smanet.validateChecksum())
      {
        smanet.packet_send_counter++;
        innerstate++;
      }
      else
        innerstate = 0;
    }
    break;

  case 2:
    //value will contain instant/spot AC power generation along with date/time of reading...
    // memcpy(&datetime, &level1packet[40 + 1 + 4], 4);
    bool validPcktID = false;

    short pcktcount = LocalUtil::get_short(smanet.level1packet + 25);
    unsigned short rcvpcktID = LocalUtil::get_short(smanet.level1packet + 27) & 0x7FFF;

    //was it my packet ? 
    uint16_t aSYSyID = LocalUtil::get_short(smanet.level1packet + 15);
    uint32_t aSerial = LocalUtil::get_long(smanet.level1packet+17);
    if (((uint16_t) aSYSyID == devSUSyID) && ((uint32_t)aSerial == devSerial)) 
    {
        validPcktID = true;
        int32_t value = 0;
        int64_t value64 = 0;
        uint32_t recordsize = 4 * ((uint32_t)smanet.level1packet[5] - 9) / ((uint32_t)LocalUtil::get_long(smanet.level1packet + 37) - (uint32_t)LocalUtil::get_long(smanet.level1packet + 33) + 1);
        for (int ii = 41; ii < smanet.packetposition - 3; ii += recordsize) 
        {
          uint8_t *recptr = smanet.level1packet + ii;
          uint32_t code = ((uint32_t)LocalUtil::get_long(recptr));
          LriDef lri = (LriDef)(code & 0x00FFFF00);
          uint32_t cls = code & 0xFF;
          uint8_t dataType = code >> 24;
          time_t datetime = (time_t)LocalUtil::get_long(recptr + 4);

          if (recordsize == 16)
          {
              value64 = LocalUtil::get_longlong(recptr + 8);
              if (is_NaN(value64) || is_NaN((uint64_t)value64))
                  value64 = 0;
          }
          else if ((dataType != DT_STRING) && (dataType != DT_STATUS))
          {
              value = LocalUtil::get_long(recptr + 16);
              if (is_NaN(value) || is_NaN((uint32_t)value))
                  value = 0;
          }

          logD("received valuetype :%%.%dX ", lri);

          switch (lri)
          {
          case DcMsWatt: ////SPOT_PDC1 / SPOT_PDC2
              //This function gives us the time when the inverter was switched off
              thisvalue = value;
              logI("DC Pwr= %li " , thisvalue);
              _client.publish(MQTT_BASE_TOPIC "instant_dc", LocalUtil::uint64ToString(thisvalue), true);
              break;

          case DcMsVol: //SPOT_UDC1 / SPOT_UDC2
              //This function gives us the time when the inverter was switched off
              thisvalue = value;
              logI("DC V= %li " , thisvalue);
              _client.publish(MQTT_BASE_TOPIC "instant_vdc", LocalUtil::uint64ToString(thisvalue), true);
              break;

          case DcMsAmp: //SPOT_IDC1 / SPOT_IDC2
              //This function gives us the time when the inverter was switched off
              thisvalue = value;
              logI("DC A= %li " , thisvalue);
              _client.publish(MQTT_BASE_TOPIC "instant_adc", LocalUtil::uint64ToString(thisvalue), true);
              break;
          default:
             break;
          }
        }
    }

    innerstate++;
    break;

    return true;
  }

  return false;
}


//-------------------------------------------------------------------------------------------
bool ESP32_SMA_Inverter::checkIfNeedToSetInverterTime()
{
  //digitalClockDisplay(now());Serial.println("");
  //digitalClockDisplay(datetime);Serial.println("");
  logI("checkIfNeedToSetInverterTime()");

  unsigned long timediff;

  timediff = abs((long) (datetime - esp32rtc.getEpoch()));
  logI("Time diff: %lu, datetime: %lu, epoch: %lu " , timediff, datetime, esp32rtc.getEpoch());

  if (timediff > 60)
  {
    //If inverter clock is out by more than 1 minute, set it to the time from NTP, saves filling up the
    //inverters event log with hundred of "change time" lines...
    setInverterTime(); //Set inverter time to now()
  }

  return true;
}



void ESP32_SMA_Inverter::setInverterTime()
{
  //Sets inverter time for those SMA inverters which don't have a realtime clock (Tripower 8000 models for instance)
  logD("setInverterTime(%i) " , innerstate);

  //Payload...

  //** 8C 0A 02 00 F0 00 6D 23 00 00 6D 23 00 00 6D 23 00
  //   9F AE 99 4F   ==Thu, 26 Apr 2012 20:22:55 GMT  (now)
  //   9F AE 99 4F   ==Thu, 26 Apr 2012 20:22:55 GMT  (now)
  //   9F AE 99 4F   ==Thu, 26 Apr 2012 20:22:55 GMT  (now)
  //   01 00         ==Timezone +1 hour for BST ?
  //   00 00
  //   A1 A5 99 4F   ==Thu, 26 Apr 2012 19:44:33 GMT  (strange date!)
  //   01 00 00 00
  //   F3 D9         ==Checksum
  //   7E            ==Trailer

  //Set time to Feb

  //2A 20 63 00 5F 00 B1 00 0B FF B5 01
  //7E 5A 00 24 A3 0B 50 DD 09 00 FF FF FF FF FF FF 01 00
  //7E FF 03 60 65 10 A0 FF FF FF FF FF FF 00 00 78 00 6E 21 96 37 00 00 00 00 00 00 01
  //** 8D 0A 02 00 F0 00 6D 23 00 00 6D 23 00 00 6D 23 00
  //14 02 2B 4F ==Thu, 02 Feb 2012 21:37:24 GMT
  //14 02 2B 4F ==Thu, 02 Feb 2012 21:37:24 GMT
  //14 02 2B 4F  ==Thu, 02 Feb 2012 21:37:24 GMT
  //00 00        ==No time zone/BST not applicable for Feb..
  //00 00
  //AD B1 99 4F  ==Thu, 26 Apr 2012 20:35:57 GMT
  //01 00 00 00
  //F6 87        ==Checksum
  //7E

  //2A 20 63 00 5F 00 B1 00 0B FF B5 01
  //7E 5A 00 24 A3 0B 50 DD 09 00 FF FF FF FF FF FF 01 00
  //7E FF 03 60 65 10 A0 FF FF FF FF FF FF 00 00 78 00 6E 21 96 37 00 00 00 00 00 00 1C
  //** 8D 0A 02 00 F0 00 6D 23 00 00 6D 23 00 00 6D 23 00
  //F5 B3 99 4F
  //F5 B3 99 4F
  //F5 B3 99 4F 01 00 00 00 28 B3 99 4F 01 00 00 00
  //F3 C7 7E

  //2B 20 63 00 5F 00 DD 00 0B FF B5 01
  //7E 5A 00 24 A3 0B 50 DD 09 00 FF FF FF FF FF FF 01 00
  //7E FF 03 60 65 10 A0 FF FF FF FF FF FF 00 00 78 00 6E 21 96 37 00 00 00 00 00 00 08
  //** 80 0A 02 00 F0 00 6D 23 00 00 6D 23 00 00 6D 23 00
  //64 76 99 4F ==Thu, 26 Apr 2012 16:23:00 GMT
  //64 76 99 4F ==Thu, 26 Apr 2012 16:23:00 GMT
  //64 76 99 4F  ==Thu, 26 Apr 2012 16:23:00 GMT
  //58 4D   ==19800 seconds = 5.5 hours
  //00 00
  //62 B5 99 4F
  //01 00 00 00
  //C3 27 7E


  time_t hosttime = time(NULL);
  // Get new host time
  hosttime = time(NULL);

  time_t invCurrTime = LocalUtil::get_long(smanet.level1packet + 45);
  time_t invLastTimeSet = LocalUtil::get_long(smanet.level1packet + 49);
  int tz = LocalUtil::get_long(smanet.level1packet + 57) & 0xFFFFFFFE;
  int dst = LocalUtil::get_long(smanet.level1packet + 57) & 0x00000001;
  int magic = LocalUtil::get_long(smanet.level1packet + 61); // What's this?


 
  time_t currenttime = esp32rtc.getEpoch(); // Returns the ESP32 RTC in number of seconds since the epoch (normally 01/01/1970)
  //digitalClockDisplay(currenttime);
  smanet.writePacketHeader(smanet.level1packet);
  smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x10, 0xA0, 0, anySUSyID, anySerial); //smanet.writeSMANET2PlusPacket(smanet.level1packet, 0x09, 0x00, smanet.packet_send_counter, 0, 0, 0);

  smanet.writeSMANET2Long(smanet.level1packet, 0xF000020A);
  smanet.writeSMANET2Long(smanet.level1packet, 0x00236D00);
  smanet.writeSMANET2Long(smanet.level1packet, 0x00236D00);
  smanet.writeSMANET2Long(smanet.level1packet, 0x00236D00);

  smanet.writeSMANET2Long(smanet.level1packet, hosttime);
  smanet.writeSMANET2Long(smanet.level1packet, hosttime);
  smanet.writeSMANET2Long(smanet.level1packet, hosttime);
  smanet.writeSMANET2Long(smanet.level1packet, tz | dst);
  smanet.writeSMANET2Long(smanet.level1packet, ++magic);
  smanet.writeSMANET2Long(smanet.level1packet, 1);

  smanet.writeSMANET2PlusPacketTrailer(smanet.level1packet);
  smanet.writePacketLength(smanet.level1packet);
  smanet.sendPacket(smanet.level1packet);
  smanet.packet_send_counter++;
  //debugMsgln(" done");
}

//passthrough 
bool ESP32_SMA_Inverter::start() {
  return smanet.start();
}
bool ESP32_SMA_Inverter::isConnected() {
  return smanet.isConnected();
}



