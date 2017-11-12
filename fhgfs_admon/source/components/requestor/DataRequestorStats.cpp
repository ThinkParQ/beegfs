#include "DataRequestorStats.h"

#include <clientstats/ClientStatsHelper.h>
#include <program/Program.h>
#include <app/App.h>


int DataRequestorStats::requestLoop()
{
   App* app = Program::getApp();

   bool statsRes;

   // do loop until terminate is set or shouldStop is set
   while (!getSelfTerminate() && !(getStatus() == DataRequestorStatsStatus_STOPPING) )
   {
      NodeStoreServers* nodeStore;
      switch (this->cfgOptions.nodeType)
      {
         case NODETYPE_Meta:
            { nodeStore = app->getMetaNodes(); }; break;
         case NODETYPE_Storage:
            { nodeStore = app->getStorageNodes(); }; break;
         default:
         {
            log.log(Log_ERR, "Only meta and storage nodetypes supported.");
            return APPCODE_INVALID_CONFIG;
         }
         break;
      }


      SafeMutexLock lock(&this->accessDataMutex);        // L O C K

      // check if requestor is needed any more, check if the last request is older then
      // 10x update interval
      unsigned unsusedTimeLimit = 10 * this->cfgOptions.intervalSecs * 1000;
      Time timeNow;
      unsigned timeDiff = timeNow.elapsedSinceMS(&this->lastRequest);
      if (timeDiff > unsusedTimeLimit)
      {
         this->stop();
         log.log(Log_DEBUG, "Stop requestor " + StringTk::uintToStr(id) + ". No client request "
            "happens during the last " + StringTk::intToStr(10 * this->cfgOptions.intervalSecs) +
            " seconds");

         lock.unlock();                                  // U N L O C K

         break;
      }

      stats->currentToOldVectorMap();
      statsRes = ClientStatsHelper::getStatsFromNodes(nodeStore, this->stats);
      if (!statsRes)
         log.log(Log_ERR, "Could not get client stats from Nodes.");

      this->dataSequenceID++;

      lock.unlock();                                     // U N L O C K

      // do nothing but wait for the time of queryInterval
      if (PThread::waitForSelfTerminateOrder(this->cfgOptions.intervalSecs * 1000))
         break;
   }

   return APPCODE_NO_ERROR;
}

void DataRequestorStats::run()
{
   try
   {
      log.log(Log_DEBUG, "Component started.");

      if (!setStatusToRunning() )
         return;

      registerSignalHandler();
      requestLoop();

      log.log(Log_DEBUG, "Component stopped.");

      setStatusToStopped();
   }
   catch (std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }
}

StatsAdmon* DataRequestorStats::getStatsCopy(uint64_t &outDataSequenceID)
{
   SafeMutexLock lock(&this->accessDataMutex);        // L O C K

   StatsAdmon* retStats = new StatsAdmon(this->stats);

   outDataSequenceID = this->dataSequenceID;
   this->lastRequest.setToNow();

   lock.unlock();                                     // U N L O C K

   return retStats;
}

bool DataRequestorStats::setStatusToRunning()
{
   bool retVal = false;

   SafeMutexLock lock(&statusMutex);              // L O C K

   if(!DATAREQUESTORSTATSSTATUS_IS_ACTIVE(this->status) )
   {
      this->status = DataRequestorStatsStatus_RUNNING;
      this->statusChangeCond.broadcast();

      retVal = true;
   }

   lock.unlock();                                 // U N L O C K

   return retVal;
}

void DataRequestorStats::setStatusToStopped()
{
   SafeMutexLock lock(&statusMutex);              // L O C K

   this->status = DataRequestorStatsStatus_STOPPED;
   this->statusChangeCond.broadcast();

   lock.unlock();                                 // U N L O C K
}

/*
 * aborts the data collector, will be used if SIGINT received
 *
 */
void DataRequestorStats::shutdown()
{
   this->selfTerminate();
}

void DataRequestorStats::waitForShutdown()
{
   SafeMutexLock safeLock(&this->statusMutex);

   while(DATAREQUESTORSTATSSTATUS_IS_ACTIVE(this->status))
   {
      this->statusChangeCond.wait(&this->statusMutex);
   }

   safeLock.unlock();
}
