#include <common/toolkit/StringTk.h>
#include "UnitTk.h"

#include <cmath>


const int64_t UNITTK_SIZE_CONVERT_FACTOR = 1024;
const int64_t UNITTK_ONE_KIBIBYTE        = UNITTK_SIZE_CONVERT_FACTOR;
const int64_t UNITTK_ONE_MEBIBYTE        = UNITTK_ONE_KIBIBYTE * UNITTK_SIZE_CONVERT_FACTOR;
const int64_t UNITTK_ONE_GIBIBYTE        = UNITTK_ONE_MEBIBYTE * UNITTK_SIZE_CONVERT_FACTOR;
const int64_t UNITTK_ONE_TEBIBYTE        = UNITTK_ONE_GIBIBYTE * UNITTK_SIZE_CONVERT_FACTOR;
const int64_t UNITTK_ONE_PEBIBYTE        = UNITTK_ONE_TEBIBYTE * UNITTK_SIZE_CONVERT_FACTOR;
const int64_t UNITTK_ONE_EXBIBYTE        = UNITTK_ONE_PEBIBYTE * UNITTK_SIZE_CONVERT_FACTOR;

const int64_t UNITTK_ONE_MINUTE = 60;
const int64_t UNITTK_ONE_HOUR   = UNITTK_ONE_MINUTE * 60;
const int64_t UNITTK_ONE_DAY    = UNITTK_ONE_HOUR * 24;



int64_t UnitTk::pebibyteToByte(double pebibyte)
{
   int64_t b = (int64_t) (pebibyte * UNITTK_ONE_PEBIBYTE);
   return b;
}

int64_t UnitTk::tebibyteToByte(double tebibyte)
{
   int64_t b = (int64_t) (tebibyte * UNITTK_ONE_TEBIBYTE);
   return b;
}

int64_t UnitTk::gibibyteToByte(double gibibyte)
{
   int64_t b = (int64_t) (gibibyte * UNITTK_ONE_GIBIBYTE);
   return b;
}

int64_t UnitTk::mebibyteToByte(double mebibyte)
{
   int64_t b = (int64_t) (mebibyte * UNITTK_ONE_MEBIBYTE);
   return b;
}

int64_t UnitTk::kibibyteToByte(double kibibyte)
{
   int64_t b = (int64_t) (kibibyte * UNITTK_ONE_KIBIBYTE);
   return b;
}

double UnitTk::byteToMebibyte(double byte)
{
  return (byte / UNITTK_ONE_MEBIBYTE);
}

double UnitTk::byteToXbyte(int64_t bytes, std::string *outUnit)
{
   return byteToXbyte(bytes, outUnit, false);
}

/**
 * Convert number of bytes without a unit to the number of bytes with a unit (e.g. MiB for mebibyte)
 */
double UnitTk::byteToXbyte(int64_t bytes, std::string *outUnit, bool round)
{
   double res = bytes;

   short count = 0;
   while ( (res > UNITTK_SIZE_CONVERT_FACTOR) && (count < 6) )
   {
      res = res / UNITTK_SIZE_CONVERT_FACTOR;
      count++;
   }

   switch(count)
   {
      case 0:
         *outUnit = "Byte";
         break;
      case 1:
         *outUnit = "KiB";
         break;
      case 2:
         *outUnit = "MiB";
         break;
      case 3:
         *outUnit = "GiB";
         break;
      case 4:
         *outUnit = "TiB";
         break;
      case 5:
         *outUnit = "PiB";
         break;
      case 6:
         *outUnit = "EiB";
         break;
   }

   if(round)
   {
      res = floor(res * 10 + 0.5) / 10;
   }

   return res;
}

double UnitTk::mebibyteToXbyte(int64_t mebibyte, std::string *unit)
{
   return mebibyteToXbyte(mebibyte, unit, false);
}

double UnitTk::mebibyteToXbyte(int64_t mebibyte, std::string *unit, bool round)
{
   double res = mebibyte;

   short count = 0;
   while (res > UNITTK_SIZE_CONVERT_FACTOR && count<4)
   {
      res = (double)res / UNITTK_SIZE_CONVERT_FACTOR;
      count++;
   }

   switch(count)
   {
      case 0:
         *unit = "MiB";
         break;
      case 1:
         *unit = "GiB";
         break;
      case 2:
         *unit = "TiB";
         break;
      case 3:
         *unit = "PiB";
         break;
      case 4:
         *unit = "EiB";
         break;
   }

   if(round)
   {
      res = floor(res * 10 + 0.5) / 10;
   }

   return res;
}

/**
 * Convert number of bytes with a given unit (e.g. mebibytes) to the number of bytes without a unit.
 */
int64_t UnitTk::xbyteToByte(double xbyte, std::string unit)
{
	double res = xbyte;

	if (unit == "KiB")
	{
		res = kibibyteToByte(res);
   }
	else
	if (unit == "MiB")
	{
		res = mebibyteToByte(res);
   }
	else
	if (unit == "GiB")
	{
		res = gibibyteToByte(res);
   }
   else
   if (unit == "TiB")
   {
      res = tebibyteToByte(res);
   }
   else
   if (unit == "PiB")
   {
      res = pebibyteToByte(res);
   }

	return (int64_t)res;
}

/**
 * Transforms an integer with a unit appended to its non-human equivalent,
 * e.g. "1K" will return 1024.
 */
int64_t UnitTk::strHumanToInt64(const char* s)
{
   size_t sLen = strlen(s);

   if(sLen < 2)
      return StringTk::strToInt64(s);

   char unit = s[sLen-1];

   if( (unit == 'P') || (unit == 'p') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_PEBIBYTE;
   }
   else
   if( (unit == 'T') || (unit == 't') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_TEBIBYTE;
   }
   else
   if( (unit == 'G') || (unit == 'g') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_GIBIBYTE;
   }
   else
   if( (unit == 'M') || (unit == 'm') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_MEBIBYTE;
   }
   if( (unit == 'K') || (unit == 'k') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_KIBIBYTE;
   }
   else
      return StringTk::strToInt64(s);
}


/**
 * Prints the number with a unit appended, but only if it matches without a remainder
 * (e.g. 1025 will not be printed as 1k)
 */
std::string UnitTk::int64ToHumanStr(int64_t a)
{
   char aStr[24];

   if( ( (a >= UNITTK_ONE_EXBIBYTE) || (-a >= UNITTK_ONE_EXBIBYTE) ) &&
       ( (a % UNITTK_ONE_EXBIBYTE) == 0) )
   {
      sprintf(aStr, "%qdE", (long long)a / UNITTK_ONE_EXBIBYTE);
   }
   else
   if( ( (a >= UNITTK_ONE_PEBIBYTE) || (-a >= UNITTK_ONE_PEBIBYTE) ) &&
       ( (a % UNITTK_ONE_PEBIBYTE) == 0) )
   {
      sprintf(aStr, "%qdP", (long long)a / UNITTK_ONE_PEBIBYTE);
   }
   else
   if( ( (a >= UNITTK_ONE_TEBIBYTE) || (-a >= UNITTK_ONE_TEBIBYTE) ) &&
       ( (a % UNITTK_ONE_TEBIBYTE) == 0) )
   {
      sprintf(aStr, "%qdT", (long long)a / UNITTK_ONE_TEBIBYTE);
   }
   else
   if( ( (a >= UNITTK_ONE_GIBIBYTE) || (-a >= UNITTK_ONE_GIBIBYTE) ) &&
       ( (a % UNITTK_ONE_GIBIBYTE) == 0) )
   {
      sprintf(aStr, "%qdG", (long long)a / UNITTK_ONE_GIBIBYTE);
   }
   else
   if( ( (a >= UNITTK_ONE_MEBIBYTE) || (-a >= UNITTK_ONE_MEBIBYTE) ) &&
       ( (a % UNITTK_ONE_MEBIBYTE) == 0) )
   {
      sprintf(aStr, "%qdM", (long long)a / UNITTK_ONE_MEBIBYTE);
   }
   else
   if( ( (a >= UNITTK_ONE_KIBIBYTE) || (-a >= UNITTK_ONE_KIBIBYTE) ) &&
       ( (a % UNITTK_ONE_KIBIBYTE) == 0) )
   {
      sprintf(aStr, "%qdK", (long long)a / UNITTK_ONE_KIBIBYTE);
   }
   else
      sprintf(aStr, "%qd", (long long)a);

   return aStr;
}

/**
 * checks if the given human string is valid an can processed by strHumanToInt64()
 *
 * @param humanString the human string to validate
 */
bool UnitTk::isValidHumanString(std::string humanString)
{
   size_t humanStringLen = humanString.length();
   if(!humanStringLen)
      return false;

   if(StringTk::isNumeric(humanString))
      return true;

   char unit = humanString.at(humanStringLen-1);
   std::string value = humanString.substr(0, humanStringLen-1);
   return StringTk::isNumeric(value) && ( (unit == 'E') || (unit == 'e') ||
      (unit == 'P') || (unit == 'p') || (unit == 'T') || (unit == 't') ||
      (unit == 'G') || (unit == 'g') || (unit == 'M') || (unit == 'm') ||
      (unit == 'K') || (unit == 'k') );
}

/**
 * Transforms an integer with a time unit appended to its non-human equivalent in seconds,
 * e.g. "1h" will return 3600.
 */
int64_t UnitTk::timeStrHumanToInt64(const char* s)
{
   size_t sLen = strlen(s);

   if(sLen < 2)
      return StringTk::strToInt64(s);

   char unit = s[sLen-1];

   if( (unit == 'D') || (unit == 'd') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_DAY;
   }
   else
   if( (unit == 'H') || (unit == 'h') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_HOUR;
   }
   else
   if( (unit == 'M') || (unit == 'm') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * UNITTK_ONE_MINUTE;
   }
   else
      return StringTk::strToInt64(s);
}

/**
 * checks if the given human string is a valid time and can processed by timeStrHumanToInt64()
 *
 * @param humanString the human string to validate
 */
bool UnitTk::isValidHumanTimeString(std::string humanString)
{
   size_t humanStringLen = humanString.length();
   if(!humanStringLen)
      return false;

   if(StringTk::isNumeric(humanString))
      return true;

   char unit = humanString.at(humanStringLen-1);
   std::string value = humanString.substr(0, humanStringLen-1);
   return StringTk::isNumeric(value) && ( (unit == 'd') || (unit == 'D') || (unit == 'H') ||
      (unit == 'h') || (unit == 'M') || (unit == 'm') || (unit == 'S') || (unit == 's') );
}

uint64_t UnitTk::quotaBlockCountToByte(uint64_t quotaBlockCount, QuotaBlockDeviceFsType type)
{
   static uint64_t quotaBlockSizeXFS = 512;

   // the quota block size for XFS is 512 bytes, for ext4 the quota block size is 1 byte
   if(type == QuotaBlockDeviceFsType_XFS)
   {
      quotaBlockCount = quotaBlockCount * quotaBlockSizeXFS;
   }

   return quotaBlockCount;
}

/*
 * Prints the quota block count as a number with a unit appended, but only if it matches without a
 * remainder. A quota block has a size of 1024 B.
 */
std::string UnitTk::quotaBlockCountToHumanStr(uint64_t quotaBlockCount, QuotaBlockDeviceFsType type)
{
   std::string unit;

   quotaBlockCount = quotaBlockCountToByte(quotaBlockCount, type);

   double value = byteToXbyte(quotaBlockCount, &unit);

   return StringTk::doubleToStr(value) + " " + unit;
}
