/*
 NANODE SMA PV MONITOR
 Bluetooth.ino 
 */

#include "Arduino.h"
#include <time.h>
#include "SMANetArduino.h"
#include "BluetoothSerial.h"
#include <EEPROM.h>
#include <esp_bt_device.h> // ESP32 BLE
#include <logging.hpp>
#include "Utils_SMA.h"
#include "site_details.h"

using namespace esp32m;

// Setup the Bluetooth connection, returns true on success, false otherwise
// On fail, should be called again.
bool ESP32SmaBluetooth::BTStart()
{
  log_d("BTStart(%i)", btstate);

  if (btstate == STATE_FRESH)
  {
    SerialBT.begin("ESP32test", true); // "true" creates this device as a BT Master.
    SerialBT.setPin("0000");           // pin as in "PIN" This is the BT connection pin, not login pin. ALWAYS 0000, unchangable.
    updateMyDeviceAddress();
    const uint8_t *addr = esp_bt_dev_get_address();
    log_d("My BT Address: %s  SMA BT Address:  %s ", getDeviceAddress(addr).c_str(), getDeviceAddress(address).c_str()) ;
    log_d("The SM32 started in master mode. Now trying to connect to SMA inverter.");
    SerialBT.connect(address);
  }

  if (SerialBT.connected(1))
  {
    btstate = STATE_CONNECTED;
    log_w("Bluetooth connected succesfully!");
    return true;
  }
  else
  {
    btstate = STATE_SETUP;
    log_e("Failed to connect. Make sure remote device is available and in range, then restart app.");
    return false;
  }
}

bool ESP32SmaBluetooth::BTCheckConnected()
{
  return SerialBT.connected(1);
}

String ESP32SmaBluetooth::getDeviceAddress(const uint8_t *point) {
  String daString("00:00:00:00:00:00");
  for (int i = 0; i < 6; i++)
  {
    char str[3];
    sprintf(str, "%02X", (int)point[i]);
    daString.concat(str);

    if (i < 5)
    {
      daString.concat(":");
    }
  }
  return daString;
}

void ESP32SmaBluetooth::updateMyDeviceAddress()
{
  const uint8_t *point = esp_bt_dev_get_address();
  for (int i = 0; i < 6; i++)
  {
    // Save address in reverse because ¯\_(ツ)_/¯
    myBTAddress[i] = (char)point[5 - i];
  }
}

void ESP32SmaBluetooth::sendPacket(unsigned char *btbuffer, unsigned int pposition )
{
  SerialBT.write(btbuffer, pposition);
}

void ESP32SmaBluetooth::writeArrayIntoEEPROM(unsigned char readbuffer[], int length, int EEPROMoffset)
{
  //Writes an array into EEPROM and calculates a simple XOR checksum
  byte checksum = 0;
  for (int i = 0; i < length; i++)
  {
    EEPROM.write(EEPROMoffset + i, readbuffer[i]);
    //Serial.print(EEPROMoffset+i); Serial.print("="); Serial.println(readbuffer[i],HEX);
    checksum ^= readbuffer[i];
  }

  //Serial.print(EEPROMoffset+length); Serial.print("="); Serial.println(checksum,HEX);
  EEPROM.write(EEPROMoffset + length, checksum);
}

bool ESP32SmaBluetooth::readArrayFromEEPROM(unsigned char readbuffer[], int length, int EEPROMoffset)
{
  //Writes an array into EEPROM and calculates a simple XOR checksum
  byte checksum = 0;
  for (int i = 0; i < length; i++)
  {
    readbuffer[i] = EEPROM.read(EEPROMoffset + i);
    //Serial.print(EEPROMoffset+i); Serial.print("="); Serial.println(readbuffer[i],HEX);
    checksum ^= readbuffer[i];
  }
  //Serial.print(EEPROMoffset+length); Serial.print("="); Serial.println(checksum,HEX);
  return (checksum == EEPROM.read(EEPROMoffset + length));
}

unsigned char ESP32SmaBluetooth::getByte()
{
  //Returns a single byte from the bluetooth stream (with error timeout/reset)
  unsigned long time;
  int inInt = 0; // ESP32 SerialBT.read() returns an int, not a char.

  //Max wait 60 seconds, before throwing an fatal error
  time = 60000 + millis();

  while (!SerialBT.available())
  {
    delay(5); //Wait for BT byte to arrive
    if (millis() > time)
    {
      log_w("Timeout");
      error();
    }
  }

  //if( (millis() - charTime) > 1 ) Serial.println(); // Breaks up SMA's messages into lines.
  inInt = SerialBT.read();
  //charTime = millis(); // Used to detect a gap between messages (so can print \r\n).

  if (inInt == -1)
  {
    log_e("ERROR: Asked for a BT char when there was none to get.");
    return '!';
  }
  else
  {
    //Serial.print( (unsigned char)inInt, HEX );
    //Serial.print(' ');
    return (unsigned char)inInt;
  }
}

void ESP32SmaBluetooth::convertBTADDRStringToArray(char *tempbuf, unsigned char *outarray, char match)
{
  //Convert BT Address into a more useful byte array, the BT address is in a really awful format to parse!
  //Must be a better way of doing this function!

  int l = strlen(tempbuf);
  char *firstcolon = strchr(tempbuf, match) + 1;
  char *lastcolon = strrchr(tempbuf, match) + 1;

  //Could use a shared buffer here to save RAM
  char output[13] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', 0};

  //Deliberately avoided sprintf as it adds over 1600bytes to the program size at compile time.
  int partlen = (firstcolon - tempbuf) - 1;
  strncpy(output + (4 - partlen), tempbuf, partlen);

  partlen = (lastcolon - firstcolon) - 1;
  strncpy(output + 4 + (2 - partlen), firstcolon, partlen);

  partlen = l - (lastcolon - tempbuf);
  strncpy(output + 6 + (6 - partlen), lastcolon, partlen);

  //Finally convert the string (11AABB44FF66) into a real byte array
  //written backwards in the same format that the SMA packets expect it in
  int i2 = 5;
  for (int i = 0; i < 12; i += 2)
  {
    outarray[i2--] = hex2bin(&output[i]);
  }
}

int hex2bin(const char *s)
{
  int ret = 0;
  int i;
  for (i = 0; i < 2; i++)
  {
    char c = *s++;
    int n = 0;
    if ('0' <= c && c <= '9')
      n = c - '0';
    else if ('a' <= c && c <= 'f')
      n = 10 + c - 'a';
    else if ('A' <= c && c <= 'F')
      n = 10 + c - 'A';
    ret = n + ret * 16;
  }
  return ret;
}
