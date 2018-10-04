#include "StringTk.h"
#include "Random.h"

std::string StringTk::trim(const std::string s)
{
   ssize_t sLength = s.length();

   ssize_t firstLeft = 0;
   while( (firstLeft < sLength) &&
      (s[firstLeft]==' ' || s[firstLeft]=='\n' || s[firstLeft]=='\r' || s[firstLeft]=='\t') )
      firstLeft++;

   if(firstLeft == sLength)
      return ""; // the string is empty or contains only space chars

   // the string contains at least one non-space char

   ssize_t lastRight = sLength - 1;
   while(s[lastRight]==' ' || s[lastRight]=='\n' || s[lastRight]=='\r' || s[lastRight]=='\t')
      lastRight--;

   return s.substr(firstLeft, lastRight - firstLeft + 1);
}

/**
 * Note: Does not perform any trimming or length checks to avoid empty elements, use explodeEx
 * if you need trimming.
 */
void StringTk::explode(const std::string s, char delimiter, StringList* outList)
{
   explodeEx(s, delimiter, false, outList);
}

/**
 * Note: Does not perform any trimming or length checks to avoid empty elements, use explodeEx
 * if you need trimming.
 */
void StringTk::explode(const std::string s, char delimiter, StringVector* outVec)
{
   explodeEx(s, delimiter, false, outVec);
}

/**
 * @param useTrim true to leave out empty elements and trim spaces
 */
void StringTk::explodeEx(const std::string s, char delimiter, bool useTrim, StringList* outList)
{
   std::string::size_type lastPos = (s.length() && (s[0] == delimiter) ) ? 0 : -1;

   // note: we ignore the first (empty) string if the input string starts with a delimiter.
   //    well actually, we ignore any emptry string...

   for(std::string::size_type currentPos = 1; ; )
   {
      currentPos = s.find(delimiter, lastPos+1);

      if(currentPos == std::string::npos)
      {
         // add rest
         std::string newElem = s.substr(lastPos+1);

         if(useTrim)
            newElem = trim(newElem);

         if(!newElem.empty() )
            outList->push_back(newElem);
         return;
      }

      // add substring to outList (leave the delimiter positions out)
      std::string newElem = s.substr(lastPos+1, currentPos-lastPos-1); // -1=last delimiter

      if(useTrim)
         newElem = trim(newElem);

      if(!newElem.empty() )
         outList->push_back(newElem);

      lastPos = currentPos;
   }
}

/**
 * @param useTrim true to leave out empty elements and trim spaces
 */
void StringTk::explodeEx(const std::string s, char delimiter, bool useTrim, StringVector* outVec)
{
   std::string::size_type lastPos = (s.length() && (s[0] == delimiter) ) ? 0 : -1;

   // note: we ignore the first (empty) string if the input string starts with a delimiter.
   //    well actually, we ignore any emptry string...

   for(std::string::size_type currentPos = 1; ; )
   {
      currentPos = s.find(delimiter, lastPos+1);

      if(currentPos == std::string::npos)
      {
         // add rest
         std::string newElem = s.substr(lastPos+1);

         if(useTrim)
            newElem = trim(newElem);

         if(!newElem.empty() )
            outVec->push_back(newElem);
         return;
      }

      // add substring to outList (leave the delimiter positions out)
      std::string newElem = s.substr(lastPos+1, currentPos-lastPos-1); // -1=last delimiter

      if(useTrim)
         newElem = trim(newElem);

      if(!newElem.empty() )
         outVec->push_back(newElem);

      lastPos = currentPos;
   }
}

/*
 * creates a delimiter-seperated list from a StringList
 *
 * @param delimiter a char to be used as delimiter
 * @param inList a StringList
 * @param useTrim true to leave out empty elements and trim spaces
 */
std::string StringTk::implode(char delimiter, StringList& inList, bool useTrim)
{
   std::string outStr;

   if(inList.empty() )
   {
      return outStr;
   }

   for(StringListIter iter = inList.begin(); iter != inList.end(); iter++)
   {
      std::string currentElem;;

      if(!useTrim)
         currentElem = *iter;
      else
      { // trim and skip empty lines
         currentElem = trim(*iter);

         if(currentElem.empty() )
            continue;
      }

      outStr += currentElem;
      outStr += delimiter;
   }

   // resize string to strip the trailing delimiter
   if(!outStr.empty() )
      outStr.resize(outStr.length()-1);

   return outStr;
}

int StringTk::strToInt(const char* s)
{
   return atoi(s); // NOLINT users depend on shady error behaviour
}

unsigned StringTk::strToUInt(const char* s)
{
   unsigned retVal = 0;

   sscanf(s, "%u", &retVal); // NOLINT users depend on shady error behaviour

   return retVal;
}

unsigned StringTk::strHexToUInt(const char* s)
{
   unsigned retVal = 0;

   sscanf(s, "%X", &retVal); // NOLINT users depend on shady error behaviour

   return retVal;
}

unsigned StringTk::strOctalToUInt(const char* s)
{
   unsigned retVal = 0;

   sscanf(s, "%o", &retVal); // NOLINT users depend on shady error behaviour

   return retVal;
}


int64_t StringTk::strToInt64(const char* s)
{
   return atoll(s); // NOLINT users depend on shady error behaviour
}

uint64_t StringTk::strToUInt64(const char* s)
{
   unsigned long long retVal = 0;

   sscanf(s, "%Lu", &retVal); // NOLINT users depend on shady error behaviour

   return retVal;
}

/**
 * note: retruns true for empty strings, which is important for command line parsing, e.g. is user
 * gives an argument like "enableXY" (with an implicit empty string) as short-hand for
 * "enableXY=true"
 */
bool StringTk::strToBool(const char* s)
{
   return !(s[0]) ||
       !strcmp(s, "1") ||
       !strcasecmp(s, "y") ||
       !strcasecmp(s, "on") ||
       !strcasecmp(s, "yes") ||
       !strcasecmp(s, "true");
}

std::string StringTk::intToStr(int a)
{
   char aStr[24];
   snprintf(aStr, 24, "%d", a);

   return aStr;
}

std::string StringTk::uintToStr(unsigned a)
{
   char aStr[24];
   snprintf(aStr, 24, "%u", a);

   return aStr;
}

std::string StringTk::uintToHexStr(unsigned a)
{
   char aStr[24];
   snprintf(aStr, 24, "%X", a);

   return aStr;
}

std::string StringTk::uint16ToHexStr(uint16_t a)
{
   char aStr[24];
   snprintf(aStr, 24, "%X", a);

   return aStr;
}

std::string StringTk::int64ToStr(int64_t a)
{
   char aStr[24];
   snprintf(aStr, 24, "%qd", (long long)a);

   return aStr;
}

std::string StringTk::uint64ToStr(uint64_t a)
{
   char aStr[24];
   snprintf(aStr, 24, "%qu", (unsigned long long)a);

   return aStr;
}

std::string StringTk::uint64ToHexStr(uint64_t a)
{
   char aStr[24];
   snprintf(aStr, 24, "%qX", (unsigned long long)a);

   return aStr;
}

std::string StringTk::doubleToStr(double a)
{
   char aStr[32];
   snprintf(aStr, 32, "%0.3lf", a);

   return aStr;
}

/**
 * @param precision number of digits after comma
 */
std::string StringTk::doubleToStr(double a, int precision)
{
   char aStr[32];
   snprintf(aStr, 32, "%.*lf", precision, a);

   return aStr;
}

std::string StringTk::timespanToStr(int64_t timespan_seconds)
{
	int seconds = 0;
	int minutes = 0;
	int hours = 0;
	int days = 0;

	std::string outStr;

	if (timespan_seconds >= 60)
	{
	  minutes = (int)(timespan_seconds/60);
	  seconds = seconds % 60;
	}
	else
	{
		seconds = timespan_seconds;
	}


	outStr += intToStr(seconds) + " seconds";


	if (minutes >= 60)
	{
	  hours = (int)(minutes/60);
	  minutes = minutes % 60;
	}

   outStr.insert(0, intToStr(minutes) + " minutes, ");


	if (hours >= 24)
	{
	  days = (int)(hours/24);
	  hours = hours % 24;
	}

   outStr.insert(0, intToStr(hours) + " hours, ");
   outStr.insert(0, intToStr(days) + " days, ");
   return outStr;
}

std::string StringTk::uint16VecToStr(const UInt16Vector* vec)
{
   char delimiter = ',';

   std::string outStr;

   if(vec->empty())
   {
      return outStr;
   }

   for(UInt16VectorConstIter iter = vec->begin(); iter != vec->end(); iter++)
   {
      outStr += StringTk::uintToStr(*iter);
      outStr += delimiter;
   }

   outStr.resize(outStr.length()-1);

   return outStr;
}

void StringTk::strToUint16Vec(std::string& s, UInt16Vector* outVec)
{
   char delimiter = ',';

   std::string::size_type lastPos = (s.length() && (s[0] == delimiter) ) ? 0 : -1;

   // note: empty strings are ignored
   for(std::string::size_type currentPos = 1; ; )
   {
      currentPos = s.find(delimiter, lastPos+1);

      if(currentPos == std::string::npos)
      {
         // add rest
         std::string newElem = s.substr(lastPos+1);
         if(newElem.length() > 0)
            outVec->push_back((int16_t)StringTk::strToUInt(newElem));
         return;
      }

      // add substring to outList (leave the delimiter positions out)
      std::string newElem = s.substr(lastPos+1, currentPos-lastPos-1); // -1=last delimiter
      if(newElem.length() > 0)
         outVec->push_back((int16_t)StringTk::strToUInt(newElem));

      lastPos = currentPos;
   }
}

/**
 * Test whether a string only contains numeric characters.
 *
 * Note: Only tests, 0..9, not the negative charachter "-".
 *
 * @return true if string is not empty and contains only numeric characters.
 */
bool StringTk::isNumeric(const std::string testString)
{
   if(testString.empty() )
      return false;

   if(testString.find_first_not_of("0123456789") != std::string::npos)
      return false;

   return true;
}

void StringTk::genRandomAlphaNumericString(std::string& inOutString, const int length)
{
   Random rand;

   static const std::string possibleValues =
           "0123456789"
           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
           "abcdefghijklmnopqrstuvwxyz";

   for (int i = 0; i < length; ++i)
      inOutString.push_back(possibleValues[rand.getNextInRange(0, possibleValues.size() - 1) ] );
}
