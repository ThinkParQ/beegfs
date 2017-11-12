#include <common/toolkit/Time.h>
#include <program/Program.h>
#include <app/App.h>
#include <app/config/Config.h>
#include "FullRefresher.h"


FullRefresher::FullRefresher()
{
   log.setContext("Refresher");
}

FullRefresher::~FullRefresher()
{
   // nothing to be done here
}

/**
 * Start refresher slave if it's not running already.
 *
 * @return true if successfully started or already running, false if startup problem occurred.
 */
bool FullRefresher::startRefreshing()
{
   const char* logContext = "Refresher (start)";
   bool retVal = true; // false if error occurred

   SafeMutexLock safeLock(&slave.statusMutex);

   if(!slave.isRunning)
   { // slave thread not running yet => start it
      slave.resetSelfTerminate();

      /* reset stats counters here (instead of slave thread) to avoid showing old values if caller
         queries status before slave thread resets them */
      slave.numDirsRefreshed = 0;
      slave.numFilesRefreshed = 0;

      try
      {
         slave.start();

         slave.isRunning = true;
      }
      catch(PThreadCreateException& e)
      {
         LogContext(logContext).logErr(std::string("Unable to start thread: ") + e.what() );
         retVal = false;
      }
   }

   safeLock.unlock();

   return retVal;
}

void FullRefresher::stopRefreshing()
{
   SafeMutexLock safeLock(&slave.statusMutex);

   if(slave.isRunning)
      slave.selfTerminate();

   safeLock.unlock();
}

void FullRefresher::waitForStopRefreshing()
{
   SafeMutexLock safeLock(&slave.statusMutex);

   while(slave.isRunning)
      slave.isRunningChangeCond.wait(&slave.statusMutex);

   safeLock.unlock();
}
