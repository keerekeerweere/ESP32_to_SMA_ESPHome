#include "LocalUtil.h"


String LocalUtil::uint64ToString(uint64_t input, uint8_t base) {
  String result = "";
  // prevent issues if called with base <= 1
  if (base < 2)
    base = 10;
  // Check we have a base that we can actually print.
  // i.e. [0-9A-Z] == 36
  if (base > 36)
    base = 10;

  // Reserve some string space to reduce fragmentation.
  // 16 bytes should store a uint64 in hex text which is the likely worst case.
  // 64 bytes would be the worst case (base 2).
  result.reserve(16);

  do
  {
    char c = input % base;
    input /= base;

    if (c < 10)
      c += '0';
    else
      c += 'A' - 10;
    result = c + result;
  } while (input);
  return result;
}

short LocalUtil::get_short(unsigned char *buf)
{
    register short shrt = 0;

    shrt += *(buf+1);
    shrt <<= 8;
    shrt += *(buf);

    return shrt;
}

int32_t LocalUtil::get_long(unsigned char *buf) {
  int32_t lng = 0;

  memcpy(&lng, buf, 4);

  if ((lng == (int32_t)NaN_S32) || (lng == (int32_t)NaN_U32))
    lng = 0;
  return lng;
}


int64_t LocalUtil::get_longlong(unsigned char *buf)
{
    register int64_t lnglng = 0;

    lnglng += *(buf+7);
    lnglng <<= 8;
    lnglng += *(buf+6);
    lnglng <<= 8;
    lnglng += *(buf+5);
    lnglng <<= 8;
    lnglng += *(buf+4);
    lnglng <<= 8;
    lnglng += *(buf+3);
    lnglng <<= 8;
    lnglng += *(buf+2);
    lnglng <<= 8;
    lnglng += *(buf+1);
    lnglng <<= 8;
    lnglng += *(buf);

    return lnglng;
}
