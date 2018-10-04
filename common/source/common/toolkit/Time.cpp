
#include <common/system/System.h>
#include "Time.h"
#include "TimeException.h"

clockid_t Time::clockID = TIME_DEFAULT_CLOCK_ID;


/**
 * Test the default clock type and switch to another type if it does not work
 */
bool Time::testClock()
{
   struct timespec now;

   while (true)
   {
      int getTimeRes = clock_gettime(clockID, &now);
      if (getTimeRes)
      {
         if (clockID != TIME_SAFE_CLOCK_ID)
         {
            clockID = TIME_SAFE_CLOCK_ID;
            continue;
         }

         std::string sysErr = System::getErrString();
         throw TimeException("clock_gettime() does not work! Error: " + sysErr);
         return false; // getting the time does not work, we need to throw an exception
      }
      else
         return true;
   }
}

/**
 * Test if the given clockID works.
 */
bool Time::testClockID(clockid_t clockID)
{
   struct timespec now;

   while (true)
   {
      int getTimeRes = clock_gettime(clockID, &now);
      return getTimeRes == 0;
   }
}


