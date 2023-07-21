/*
 NANODE SMA PV MONITOR
 SMANetArduino.ino
 */
#include "Arduino.h"
#include <logging.hpp>
#include "SMANetArduino.h"
#include "SmaBluetooth.h"
#include "Utils_SMA.h"


using namespace esp32m;

unsigned int ESP32_SMANetArduino::readLevel1PacketFromBluetoothStream(int index)
{
  unsigned int retcmdcode;
  bool errorCodePCS;
  bool errorCodeVSA;
  bool errorCodePIFM;
  //HowMuchMemory();

  //Serial.print(F("#Read cmd="));
  //Serial.print("READ=");  Serial.println(index,HEX);
  retcmdcode = 0;
  errorCodePCS = false;
  errorCodeVSA = false;
  errorCodePIFM = false;
  //level1packet={0};

  //Wait for a start packet byte
  while (smaBluetooth.getByte() != '\x7e')
  {
    delay(1);
  }

  byte len1 = smaBluetooth.getByte();
  byte len2 = smaBluetooth.getByte();
  byte packetchecksum = smaBluetooth.getByte();

  if ((0x7e ^ len1 ^ len2) == packetchecksum)
    errorCodePCS = false;
  else
  {
    log_i("*** Invalid header chksum.");
    errorCodePCS = true;
  }

  for (int i = 0; i < 6; i++)
    Level1SrcAdd[i] = smaBluetooth.getByte();
  for (int i = 0; i < 6; i++)
    Level1DestAdd[i] = smaBluetooth.getByte();

  retcmdcode = smaBluetooth.getByte() + (smaBluetooth.getByte() * 256);
  packetlength = len1 + (len2 * 256);
  //Serial.println(" ");
  //Serial.print("packetlength = ");
  //Serial.println(packetlength);

  if (ValidateSenderAddress())
  {
    //Serial.println("*** Message is from SMA.");
    errorCodeVSA = false;
  }
  else
  {
    log_w("P wrng dest");
    errorCodeVSA = true;
  }
  if (IsPacketForMe())
  {
    //Serial.println("*** Message is to ESP32.");
    errorCodePIFM = false;
  }
  else
  {
    log_w("P wrng snder");
    errorCodePIFM = true;
  }

  //If retcmdcode==8 then this is part of a multi packet response which we need to buffer up
  //Read the whole packet into buffer
  //=0;  //Seperate index as we exclude bytes as we move through the buffer

  //Serial.println(" ");
  //Serial.print( "packetlength-level1headerlength = " );
  //Serial.println( packetlength-level1headerlength );
  for (int i = 0; i < (packetlength - level1headerlength); i++)
  {
    //Serial.print( " index = " );
    //Serial.print( index );
    //Serial.print( " " );
    level1packet[index] = smaBluetooth.getByte();

    //Keep 1st byte raw unescaped 0x7e
    //if (i>0) {
    if (escapenextbyte == true)
    {
      level1packet[index] = level1packet[index] ^ 0x20;
      escapenextbyte = false;
      index++;
    }
    else
    {
      if ((level1packet[index] == 0x7d))
      {
        //Throw away the 0x7d byte
        escapenextbyte = true;
      }
      else
        index++;
    }
  } // End of for loop that reads the ENTIRE packet into level1packet[]

  //Reset packetlength based on true length of packet
  packetlength = index + level1headerlength; // Length of the entire packet received. Which is 18 greater
  //                                               than the amount of data actually added to level1packet[]
  packetposition = index; // Index into level1packet[] of the first byte after the Level 1 header stuff.
  //                        The byte at level1packet[index] will be 0x7E for a level 2 packet. NOT 0x7E for a level 1 packet.

  if (errorCodePCS || errorCodeVSA || errorCodePIFM)
  {
    //Serial.println(" ");
    //Serial.println("readLevel1PacketFromBluetoothStream now returning with return code = 0xFFFF.");
    return 0xFFFF;
  }
  else
  {
    //Serial.println(" ");
    //Serial.print("readLevel1PacketFromBluetoothStream now returning with return code = ");
    //Serial.println(retcmdcode);
    return retcmdcode;
  }
}
//-----------------------------------------------------------
void ESP32_SMANetArduino::prepareToReceive()
{
  FCSChecksum = 0xFFFF;
  packetposition = 0;
  escapenextbyte = false;
}
//------------------------------------------------------------
bool ESP32_SMANetArduino::containsLevel2Packet()
{
  //Serial.println(" ");
  //Serial.print("packetlength = ");
  //Serial.println(packetlength);
  //Serial.print("level1headerlength = ");
  //Serial.println(level1headerlength);
  if ((packetlength - level1headerlength) < 5)
    return false;

  return (level1packet[0] == 0x7e &&
          level1packet[1] == 0xff &&
          level1packet[2] == 0x03 &&
          level1packet[3] == 0x60 &&
          level1packet[4] == 0x65);
}
//---------------------------------------------------------------
// This function will read BT packets from the inverter until it finds one that is addressed to me, and
//   has the same command code as the one I am waiting for = cmdcodetowait.
// It is called when you know a Level1 message is coming.
// NOTE: NEVER call this function with cmdcodetowait = 0xFFFF. That value is returned
//   if the packet has a checksum error, or it's not addressed to me, etc...
// This function just discards the packet if there is an error, returns true on success
// false otherwise, at which point it should be called again

unsigned int lastGetPacket = -1;
bool ESP32_SMANetArduino::getPacket(unsigned int cmdcodetowait)
{
  if (lastGetPacket != cmdcodetowait)
  {
    prepareToReceive();
    lastGetPacket = cmdcodetowait;
  }
  cmdcode = readLevel1PacketFromBluetoothStream(0);
  // debugMsg("Command code: ");
  // debugMsgLn(String(cmdcode));
  if (cmdcode == cmdcodetowait)
  {
    return true;
    lastGetPacket = -1;
  }
  return false;
}

void ESP32_SMANetArduino::waitForPacket(unsigned int xxx)
{
  // pass
}

//------------------------------------------------------------------
// This code will also look for the proper command code.
// The difference to the function above is that it will search deeply through the level1packet[] array.
// Level 2 messages (PPP messages) follow the Level 1 packet header part. Each Level 2 packet starts with
//   7E FF 03 60 65.
unsigned int lastMulticmdcode = -1;
bool ESP32_SMANetArduino::waitForMultiPacket(unsigned int cmdcodetowait)
{

  //prepareToReceive();
  //Serial.println(" ");
  prepareToReceive();
  //Serial.println("*** In waitForMultiPacket. About to call readLevel1PacketFromBluetoothStream(0) ***");
  //cmdcode = readLevel1PacketFromBluetoothStream(packetposition);
  lastMulticmdcode = readLevel1PacketFromBluetoothStream(0);
  //Serial.println(" ");
  //Serial.print("*** Done reading packet. cmdcode = ");
  //Serial.println(cmdcode);
  if (lastMulticmdcode == cmdcodetowait)
    return true;

  //Serial.println("Now returning from 'waitForMultiPacket'.");
  return false;
}

void ESP32_SMANetArduino::writeSMANET2Short(unsigned char *btbuffer, unsigned short v)
{
    writeSMANET2SingleByte(btbuffer,(unsigned char)((v >> 0) & 0xFF));
    writeSMANET2SingleByte(btbuffer,(unsigned char)((v >> 8) & 0xFF));
}

void ESP32_SMANetArduino::writeSMANET2Long(unsigned char *btbuffer, unsigned long v)
{
  writeSMANET2SingleByte(btbuffer, (unsigned char)((v >> 0) & 0XFF));
  writeSMANET2SingleByte(btbuffer, (unsigned char)((v >> 8) & 0XFF));
  writeSMANET2SingleByte(btbuffer, (unsigned char)((v >> 16) & 0xFF));
  writeSMANET2SingleByte(btbuffer, (unsigned char)((v >> 24) & 0xFF));
}

void ESP32_SMANetArduino::writeSMANET2uint(unsigned char *btbuffer, unsigned int v)
{
  writeSMANET2SingleByte(btbuffer, (unsigned char)((v >> 0) & 0XFF));
  writeSMANET2SingleByte(btbuffer, (unsigned char)((v >> 8) & 0XFF));
  //writeSMANET2SingleByte(btbuffer,(unsigned char)((v >> 16) & 0xFF)) ;
  //writeSMANET2SingleByte(btbuffer,(unsigned char)((v >> 24) & 0xFF)) ;
}

/*
void displaySpotValues(int gap) {
  unsigned long value = 0;
  unsigned int valuetype=0;
  for(int i=40+1;i<packetposition-3;i+=gap){
    valuetype = level1packet[i+1]+level1packet[i+2]*256;
    memcpy(&value,&level1packet[i+8],3);
    //valuetype 
    //0x2601=Total Yield Wh
    //0x2622=Day Yield Wh
    //0x462f=Feed in time (hours) /60*60
    //0x462e=Operating time (hours) /60*60
    //0x451f=DC Voltage  /100
    //0x4521=DC Current  /1000
    Serial.print(valuetype,HEX);
    Serial.print("=");
    memcpy(&datetime,&level1packet[i+4],4);
    digitalClockDisplay(datetime);
    Serial.print("=");Serial.println(value);
  }
}
*/


void ESP32_SMANetArduino::writeSMANET2SingleByte(unsigned char *btbuffer, unsigned char v)
{
  //Keep a rolling checksum over the payload
  FCSChecksum = (FCSChecksum >> 8) ^ pgm_read_word_near(&fcstab[(FCSChecksum ^ v) & 0xff]);

  if (v == 0x7d || v == 0x7e || v == 0x11 || v == 0x12 || v == 0x13)
  {
    btbuffer[packetposition++] = 0x7d;
    btbuffer[packetposition++] = v ^ 0x20;
  }
  else
  {
    btbuffer[packetposition++] = v;
  }
}

void ESP32_SMANetArduino::writeSMANET2Array(unsigned char *btbuffer, unsigned char bytes[], byte loopcount)
{
  //Escape the packet if needed....
  for (int i = 0; i < loopcount; i++)
  {
    writeSMANET2SingleByte(btbuffer, bytes[i]);
  } //end for
}

void ESP32_SMANetArduino::writeSMANET2ArrayFromProgmem(unsigned char *btbuffer, prog_uchar ProgMemArray[], byte loopcount)
{
  //debugMsg("writeSMANET2ArrayFromProgmem=");
  //Serial.println(loopcount);
  //Escape the packet if needed....
  for (int i = 0; i < loopcount; i++)
  {
    writeSMANET2SingleByte(btbuffer, pgm_read_byte_near(ProgMemArray + i));
  } //end for
}

/**
 * smanet.level1packet, 0x09, 0xA1, smanet.packet_send_counter, 0, 0, 0
 * 
 * 
 * uint8_t  RootDeviceAddress[6]= {0, 0, 0, 0, 0, 0};    //Hold byte array with BT address of primary inverter
uint8_t  LocalBTAddress[6] = {0, 0, 0, 0, 0, 0};      //Hold byte array with BT address of local adapter
uint8_t  addr_broadcast[6] = {0, 0, 0, 0, 0, 0};
uint8_t  addr_unknown[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
const unsigned short AppSUSyID = 125;
unsigned long  AppSerial;
const unsigned short anySUSyID = 0xFFFF;
const unsigned long anySerial = 0xFFFFFFFF;

 * 
*/
void ESP32_SMANetArduino::writeSMANET2PlusPacket(unsigned char *btbuffer, unsigned char longwords, unsigned char ctrl, unsigned short ctrl2, unsigned short dstSUSyID, unsigned long dstSerial)
{

  //This is the header for a SMANET2+ (no idea if this is the correct name!!) packet - 28 bytes in length
  lastpacketindex = packet_send_counter;

  //Start packet
  btbuffer[packetposition++] = 0x7e; //Not included in checksum //also on sbfspot buf[packetposition++] = 0x7E; 

  //Header bytes 0xFF036065
  //{0xFF, 0x03, 0x60, 0x65}
  writeSMANET2ArrayFromProgmem(btbuffer, SMANET2header, 4); //sbfspot writes : 0x656003FF, esp32: {0xFF, 0x03, 0x60, 0x65} same (in reverse order)

  // example ctrl chars 0x09, 0xA1
  writeSMANET2SingleByte(btbuffer, longwords); //0x09 //sbspot writes single bytes (updates checksum)
  writeSMANET2SingleByte(btbuffer, ctrl); // 

  writeSMANET2Short(btbuffer, dstSUSyID); //
  writeSMANET2Long(btbuffer, dstSerial); //equivalent of sixFF

  writeSMANET2Short(btbuffer, ctrl2);

  writeSMANET2Short(btbuffer, AppSUSyID);
  writeSMANET2Long(btbuffer, AppSerial);

  writeSMANET2Short(btbuffer, ctrl2);

  writeSMANET2Short(btbuffer, 0);
  writeSMANET2Short(btbuffer, 0);

  writeSMANET2Short(btbuffer, packet_send_counter | 0x8000);

}

void ESP32_SMANetArduino::writeSMANET2PlusPacketTrailer(unsigned char *btbuffer)
{
  FCSChecksum = FCSChecksum ^ 0xffff;

  btbuffer[packetposition++] = FCSChecksum & 0x00ff;
  btbuffer[packetposition++] = (FCSChecksum >> 8) & 0x00ff;
  btbuffer[packetposition++] = 0x7e; //Trailing byte

  //Serial.print("Send Checksum=");Serial.println(FCSChecksum,HEX);
}

/*void ESP32_SMANetArduino::writePacketHeader(unsigned char *btbuffer)
{
  writePacketHeader(btbuffer, 0x01, 0x00, smaBTInverterAddressArray);
}
*/

void ESP32_SMANetArduino::writePacketHeader(unsigned char *btbuffer)
{
  writePacketHeader(btbuffer, 0x01, smaBluetooth.myBTAddress, smaBTInverterAddressArray);
}

void ESP32_SMANetArduino::writePacketHeader(unsigned char *btbuffer, const unsigned int control, unsigned char *destaddress) 
{
  writePacketHeader(btbuffer, control, smaBluetooth.myBTAddress, destaddress);
}

void ESP32_SMANetArduino::writePacketHeader(unsigned char *btbuffer, unsigned char *destaddress) {
  writePacketHeader(btbuffer, 0x01, smaBluetooth.myBTAddress, destaddress);
}

void ESP32_SMANetArduino::writePacketHeader(unsigned char *btbuffer, const unsigned int control, unsigned char *myaddress, unsigned char *destaddress)
{

  if (packet_send_counter > 75)
    packet_send_counter = 1;

  FCSChecksum = 0xffff;

  packetposition = 0;

  btbuffer[packetposition++] = 0x7e;
  btbuffer[packetposition++] = 0; //len1
  btbuffer[packetposition++] = 0; //len2
  btbuffer[packetposition++] = 0; //checksum

  for (int i = 0; i < 6; i++)
    btbuffer[packetposition++] = myaddress[i]; //was before //smaBluetooth.myBTAddress[i];    
  
  for (int i = 0; i < 6; i++)
    btbuffer[packetposition++] = destaddress[i];

  btbuffer[packetposition++] = (uint8_t)(control & 0xFF);//cmd1; //cmd byte 1
  btbuffer[packetposition++] = (uint8_t)(control >> 8);//cmd2; //cmd byte 2

  //cmdcode=cmd1+(cmd2*256);
  cmdcode = 0xffff; //Just set to dummy value

  //packetposition should now = 18
}

void ESP32_SMANetArduino::writePacketLength(unsigned char *btbuffer)
{
  //  unsigned char len1=;  //TODO: Need to populate both bytes for large packets over 256 bytes...
  //  unsigned char len2=0;  //Were only doing a single byte for now...
  btbuffer[1] = packetposition;
  btbuffer[2] = 0;
  btbuffer[3] = btbuffer[0] ^ btbuffer[1] ^ btbuffer[2]; //checksum
}

void ESP32_SMANetArduino::wrongPacket()
{
  Serial.println(F("*WRONG PACKET*"));
  error();
}

void ESP32_SMANetArduino::dumpPacket(char letter)
{
  for (int i = 0; i < packetposition; i++)
  {

    if (i % 16 == 0)
    {
      Serial.println("");
      Serial.print(letter);
      Serial.print(i, HEX);
      Serial.print(":");
    }

    debugPrintHexByte(level1packet[i]);
  }
  Serial.println("");
}

void ESP32_SMANetArduino::debugPrintHexByte(unsigned char b)
{
  if (b < 16)
    Serial.print("0"); //leading zero if needed
  Serial.print(b, HEX);
  Serial.print(" ");
}
//-------------------------------------------------------------------------
bool ESP32_SMANetArduino::validateChecksum()
{
  //Checks the validity of a LEVEL2 type SMA packet

  if (!containsLevel2Packet())
  {
    //Wrong packet index received
    //Serial.println("*** in validateChecksum(). containsLevel2Packet() returned FALSE.");
    dumpPacket('R');

    Serial.print(F("Wrng L2 hdr"));
    return false;
  }
  //Serial.println("*** in validateChecksum(). containsLevel2Packet() returned TRUE.");
  //We should probably do this loop in the receive packet code

  if (level1packet[28 - 1] != lastpacketindex)
  {
    //Wrong packet index received
    dumpPacket('R');

    Serial.print(F("Wrng Pkg count="));
    Serial.println(level1packet[28 - 1], HEX);
    Serial.println(lastpacketindex, HEX);
    return false;
  }

  FCSChecksum = 0xffff;
  //Skip over 0x7e and 0x7e at start and end of packet
  for (int i = 1; i <= packetposition - 4; i++)
  {
    FCSChecksum = (FCSChecksum >> 8) ^ pgm_read_word_near(&fcstab[(FCSChecksum ^ level1packet[i]) & 0xff]);
  }

  FCSChecksum = FCSChecksum ^ 0xffff;

  //if (1==1)
  if ((level1packet[packetposition - 3] == (FCSChecksum & 0x00ff)) && (level1packet[packetposition - 2] == ((FCSChecksum >> 8) & 0x00ff)))
  {
    if ((level1packet[23 + 1] != 0) || (level1packet[24 + 1] != 0))
    {
      log_w("chksum err");
      error();
    }
    return true;
  }
  else
  {
    log_w("Invalid chk= ");
    Serial.print(F("Invalid chk="));
    Serial.println(FCSChecksum, HEX);
    dumpPacket('R');
    delay(10000);
    return false;
  }
}
//------------------------------------------------------------------------------------
/*
void InquireBlueToothSignalStrength() {
  writePacketHeader(level1packet,0x03,0x00,smaBTInverterAddressArray);
  //unsigned char a[2]= {0x05,0x00};
  writeSMANET2SingleByte(level1packet,0x05);
  writeSMANET2SingleByte(level1packet,0x00);  
  //  writePacketPayload(level1packet,a,sizeof(a));
  writePacketLength(level1packet);
  sendPacket(level1packet);
  waitForPacket(0x0004); 
  float bluetoothSignalStrength = (level1packet[4] * 100.0) / 0xff;
  Serial.print(F("BT Sig="));
  Serial.print(bluetoothSignalStrength);
  Serial.println("%");
}
*/
String ESP32_SMANetArduino::getMAC(unsigned char *addr) {
  String macRet = String("00:00:00:00:00:00");

  macRet.clear();
  for (int i = 0; i < 6; i++)
  {
    char str[3];
    sprintf(str, "%02X", (int)addr[i]);
    macRet.concat(str);
    if (i < 5)
    {
      macRet.concat(":");
    }
  }
  return macRet;
}

bool ESP32_SMANetArduino::ValidateSenderAddress()
{
  // Compares the SMA inverter address to the "from" address contained in the message.
  // Debug prints "P wrng dest" if there is no match.
  log_d("Lvel1: %s SMABT: %s ", getMAC(Level1SrcAdd).c_str(), getMAC(smaBTInverterAddressArray).c_str());

  bool ret = (Level1SrcAdd[5] == smaBTInverterAddressArray[5] &&
              Level1SrcAdd[4] == smaBTInverterAddressArray[4] &&
              Level1SrcAdd[3] == smaBTInverterAddressArray[3] &&
              Level1SrcAdd[2] == smaBTInverterAddressArray[2] &&
              Level1SrcAdd[1] == smaBTInverterAddressArray[1] &&
              Level1SrcAdd[0] == smaBTInverterAddressArray[0]);
  log_d("Sending back: %d", ret);
  return ret;
}

bool ESP32_SMANetArduino::IsPacketForMe()
{
  // Compares the ESP32 address to the received "to" address in the message.
  // Debug prints "P wrng snder" if it does not match.
  // debugMsg("My BT address: ");
  // printMAC(myBTAddress);
  // debugMsgLn("");
  // debugMsg("Level1dstaddr: ");
  // printMAC(Level1DestAdd);
  // debugMsgLn("");

  //return (ValidateDestAddress(sixzeros) || ValidateDestAddress(smaBluetooth.myBTAddress) || ValidateDestAddress(sixff));
  return ValidateDestAddress(smaBluetooth.myBTAddress);
}

bool ESP32_SMANetArduino::ValidateDestAddress(unsigned char *address)
{
  for (int i=0; i<6;i++) 
    if ( (Level1DestAdd[i]!= address[i]) && (Level1DestAdd[i]!= 0x00) && (Level1DestAdd[i]!= 0xFF) )
      return false;

  return true;
}


void ESP32_SMANetArduino::sendPacket(unsigned char *btbuffer) {
  smaBluetooth.sendPacket(btbuffer, packetposition);
}


unsigned char* ESP32_SMANetArduino::getsmaBTInverterAddressArray() {
  return smaBTInverterAddressArray;
}



bool ESP32_SMANetArduino::start(){
  return smaBluetooth.BTStart();
} //passthrough

bool ESP32_SMANetArduino::isConnected(){
  return smaBluetooth.BTCheckConnected();
} //passthrough


// Generate a unique session ID for application
unsigned long ESP32_SMANetArduino::genSessionID()
{
    srand(time(nullptr));
    return 900000000 + ((rand() << 16) + rand()) % 100000000;
}

