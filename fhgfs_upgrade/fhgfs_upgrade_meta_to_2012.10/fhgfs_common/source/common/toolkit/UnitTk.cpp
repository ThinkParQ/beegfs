#include <common/toolkit/StringTk.h>
#include "UnitTk.h"

#include <math.h>


int64_t UnitTk::gigabyteToByte(double gigabyte)
{
	int64_t b = (int64_t) (gigabyte*1024*1024*1024);
	return b;
}


int64_t UnitTk::megabyteToByte(double megabyte)
{
	int64_t b = (int64_t) (megabyte*1024*1024);
   return b;
}

int64_t UnitTk::kilobyteToByte(double kilobyte)
{
	int64_t b = (int64_t) (kilobyte*1024);
	return b;
}

double UnitTk::byteToXbyte(int64_t bytes, std::string *unit)
{
   return byteToXbyte(bytes,unit,false);
}

/**
 * Convert number of bytes without a unit to the number of bytes with a unit (e.g. megabytes).
 */
double UnitTk::byteToXbyte(int64_t bytes, std::string *unit, bool round)
{
	double res = bytes;

	short count = 0;
	while (res > 1024 && count<3)
	{
		res = res/1024;
		count++;
	}

   switch(count)
   {
      case 0:
         *unit = "Byte";
         break;
      case 1:
         *unit = "KB";
         break;
      case 2:
         *unit = "MB";
         break;
      case 3:
         *unit = "GB";
         break;
   }

   if(round)
   {
      res = floor(res * 10 + 0.5) / 10;
   }

   return res;
}

/**
 * Convert number of bytes with a given unit (e.g. megabytes) to the number of bytes without a unit.
 */
int64_t UnitTk::xbyteToByte(double xbyte, std::string unit)
{
	double res = xbyte;

	if (unit == "KB")
	{
		res = kilobyteToByte(res);
   }
	else
	if (unit == "MB")
	{
		res = megabyteToByte(res);
   }
	else
	if (unit == "GB")
	{
		res = gigabyteToByte(res);
   }

	return (int64_t)res;
}

/**
 * Transforms an integer with a unit appended to its non-human equivalent,
 * e.g. "1K" will return 1024.
 */
int64_t UnitTk::strHumanToInt64(const char* s)
{
   int64_t oneKilo = 1024;
   int64_t oneMeg  = oneKilo * 1024;
   int64_t oneGig  = oneMeg * 1024;
   int64_t oneTera = oneGig * 1024;

   size_t sLen = strlen(s);

   if(sLen < 2)
      return StringTk::strToInt64(s);

   char unit = s[sLen-1];

   if( (unit == 'T') || (unit == 't') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * oneTera;
   }
   else
   if( (unit == 'G') || (unit == 'g') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * oneGig;
   }
   else
   if( (unit == 'M') || (unit == 'm') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * oneMeg;
   }
   if( (unit == 'K') || (unit == 'k') )
   {
      std::string sNoUnit = s;
      sNoUnit.resize(sLen-1);

      return StringTk::strToInt64(sNoUnit) * oneKilo;
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
   long long oneKilo = 1024;
   long long oneMeg  = oneKilo * 1024;
   long long oneGig  = oneMeg * 1024;
   long long oneTera = oneGig * 1024;

   char aStr[24];

   if( ( (a >= oneTera) || (-a >= oneTera) ) &&
       ( (a % oneTera) == 0) )
   {
      sprintf(aStr, "%qdT", a / oneTera);
   }
   else
   if( ( (a >= oneGig) || (-a >= oneGig) ) &&
       ( (a % oneGig) == 0) )
   {
      sprintf(aStr, "%qdG", a / oneGig);
   }
   else
   if( ( (a >= oneMeg) || (-a >= oneMeg) ) &&
       ( (a % oneMeg) == 0) )
   {
      sprintf(aStr, "%qdM", a / oneMeg);
   }
   else
   if( ( (a >= oneKilo) || (-a >= oneKilo) ) &&
       ( (a % oneKilo) == 0) )
   {
      sprintf(aStr, "%qdK", a / oneKilo);
   }
   else
      sprintf(aStr, "%qd", (long long)a);

   return aStr;
}

