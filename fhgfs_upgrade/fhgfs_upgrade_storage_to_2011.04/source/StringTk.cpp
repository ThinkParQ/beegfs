#include "StringTk.h"

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
 * Note: Does not perform any trimming or length checks to avoid empty elements.
 */
void StringTk::explode(const std::string s, char delimiter, StringList* outList)
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
         if(newElem.length() > 0)
            outList->push_back(newElem);
         return;
      }

      // add substring to outList (leave the delimiter positions out)
      std::string newElem = s.substr(lastPos+1, currentPos-lastPos-1); // -1=last delimiter
      if(newElem.length() > 0)
         outList->push_back(newElem);

      lastPos = currentPos;
   }
}

std::string StringTk::implode(char delimiter, StringList& inList)
{
   std::string outStr;

   if(inList.empty() )
   {
      return outStr;
   }

   for(StringListIter iter = inList.begin(); iter != inList.end(); iter++)
   {
      outStr += *iter;
      outStr += delimiter;
   }

   return outStr.substr(0, outStr.length() );
}


int StringTk::strToInt(const char* s)
{
   return atoi(s);
}

unsigned StringTk::strToUInt(const char* s)
{
   unsigned retVal;

   sscanf(s, "%u", &retVal);

   return retVal;
}

unsigned StringTk::strHexToUInt(const char* s)
{
   unsigned retVal;

   sscanf(s, "%X", &retVal);

   return retVal;
}

unsigned StringTk::strOctalToUInt(const char* s)
{
   unsigned retVal;

   sscanf(s, "%o", &retVal);

   return retVal;
}


int64_t StringTk::strToInt64(const char* s)
{
   return atoll(s);
}

uint64_t StringTk::strToUInt64(const char* s)
{
   unsigned long long retVal;

   sscanf(s, "%qu", &retVal);

   return retVal;
}

bool StringTk::strToBool(const char* s)
{
   if( !(s[0]) ||
       !strcmp(s, "1") ||
       !strcasecmp(s, "y") ||
       !strcasecmp(s, "on") ||
       !strcasecmp(s, "yes") ||
       !strcasecmp(s, "true") )
      return true;

   return false;
}

std::string StringTk::intToStr(int a)
{
   char aStr[24];
   sprintf(aStr, "%d", a);

   return aStr;
}

std::string StringTk::uintToStr(unsigned a)
{
   char aStr[24];
   sprintf(aStr, "%u", a);

   return aStr;
}

std::string StringTk::uintToHexStr(unsigned a)
{
   char aStr[24];
   sprintf(aStr, "%X", a);

   return aStr;
}

std::string StringTk::int64ToStr(int64_t a)
{
   char aStr[24];
   sprintf(aStr, "%qd", (long long)a);

   return aStr;
}

std::string StringTk::uint64ToStr(uint64_t a)
{
   char aStr[24];
   sprintf(aStr, "%qu", (unsigned long long)a);

   return aStr;
}

std::string StringTk::uint64ToHexStr(uint64_t a)
{
   char aStr[24];
   sprintf(aStr, "%qX", (unsigned long long)a);

   return aStr;
}

std::string StringTk::doubleToStr(double a)
{
   char aStr[32];
   sprintf(aStr, "%0.3lf", a);

   return aStr;
}

std::string StringTk::doubleToStr(double a, bool stripped)
{
   if (stripped)
   {
      char aStr[32];
      sprintf(aStr, "%0.0lf", a);

      return aStr;
   }
   else return doubleToStr(a);
}

std::string StringTk::timespanToStr(int64_t timespan_seconds)
{
   int seconds = 0;
   int minutes = 0;
   int hours = 0;
   int days = 0;

   std::string outStr = "";

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
