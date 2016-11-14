#ifndef FULLREFRESHER_H_
#define FULLREFRESHER_H_

#include <common/Common.h>
#include "FullRefresherSlave.h"


/**
 * This is not a component that represents a separate thread. Instead, it contains and controls a
 * slave thread, which is started and stopped on request (i.e. it is not automatically started when
 * the app is started).
 * The slave thread will run over all local metadata and refresh/re-calculate it.
 */
class FullRefresher
{
   public:
      FullRefresher();
      ~FullRefresher();

      bool startRefreshing();
      void stopRefreshing();
      void waitForStopRefreshing();


   private:
      LogContext log;
      FullRefresherSlave slave;



   public:
      // getters & setters
      bool getIsRunning()
      {
         SafeMutexLock safeLock(&slave.statusMutex);

         bool retVal = slave.isRunning;

         safeLock.unlock();

         return retVal;
      }

      void getNumRefreshed(uint64_t* outNumDirsRefreshed, uint64_t* outNumFilesRefreshed)
      {
         SafeMutexLock safeLock(&slave.statusMutex);

         *outNumDirsRefreshed = slave.numDirsRefreshed;
         *outNumFilesRefreshed = slave.numFilesRefreshed;

         safeLock.unlock();
      }


   private:
      // getters & setters



};


#endif /* FULLREFRESHER_H_ */
