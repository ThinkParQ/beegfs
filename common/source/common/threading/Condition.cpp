
#include <common/system/System.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/TimeException.h>
#include "Condition.h"
#include "ConditionException.h"

#ifdef CLOCK_MONOTONIC_COARSE
   // Note: Don't forget to check on program start up if it works!
   clockid_t Condition::clockID = TIME_DEFAULT_CLOCK_ID;
#else
   clockid_t Condition::clockID = TIME_SAFE_CLOCK_ID;
#endif

pthread_condattr_t Condition::condAttr; // static condAttr, initialization below

bool Condition::initStaticCondAttr()
{
   int initRes = pthread_condattr_init(&condAttr);
   if (initRes)
   {
      throw ConditionException(std::string("Condition initialization failed: ") +
         System::getErrString(initRes) );
      return false;
   }

   int setRes = pthread_condattr_setclock(&condAttr, clockID);
   if (setRes)
   {
      throw ConditionException(std::string("Condition set clockID failed: ") +
         System::getErrString(setRes) );
      return false;
   }

   return true;
}

bool Condition::destroyStaticCondAttr()
{
   int condAttrRes = pthread_condattr_destroy(&condAttr);
   if (unlikely(condAttrRes) )
   {
      throw MutexException(System::getErrString(condAttrRes) );
      return false;
   }

   return true;
}

/**
 * Test if the current clockID works. If not try to switch to a safe clock ID
 */
bool Condition::testClockID()
{
   while (true)
   {
      bool testTimeRes = Time::testClockID(clockID);
      if (!testTimeRes)
      {
         clockID = TIME_SAFE_CLOCK_ID;
         continue;
      }

      int setClockAttrRes = pthread_condattr_setclock(&condAttr, clockID);
      if (setClockAttrRes)
      {
         #ifdef BEEGFS_DEBUG
            // As we are in early initialization, log via syslog?
            // std::cerr << "pthread_condattr_setclock does not accept clockID: "<<
            //   clockID << " Error: " << System::getErrString(setClockAttrRes) << std::endl;
         #endif

         // using the current clock failed
         if (clockID == TIME_SAFE_CLOCK_ID)
         {
            std::string errMsg = "pthread_condattr_setclock(" + StringTk::intToStr(clockID) +
               ") does not work";
            throw TimeException(errMsg);
            return false;
         }
         else
         {
            clockID = TIME_SAFE_CLOCK_ID;
            bool testTimeRes = Time::testClockID(clockID);
            if (!testTimeRes)
            {
               throw TimeException("Time::testClock(SAFE_CLOCK_ID) failed!");
               return false;
            }

            continue;
         }
      }
      else
      {
         #ifdef BEEGFS_DEBUG
            if (clockID != TIME_SAFE_CLOCK_ID)
               std::cout << "Good, your libc supports a fast clockIDs! ClockID: "
                  << clockID << std::endl;
         #endif
         return true; // all ok
      }
   }

   return false; // we never should end up here
}

