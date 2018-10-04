#include "ChunkFetcher.h"

#include <program/Program.h>

#include <common/Common.h>

ChunkFetcher::ChunkFetcher()
   : log("ChunkFetcher")
{
   // for each targetID, put one fetcher thread into list
   for (const auto& mapping : Program::getApp()->getStorageTargets()->getTargets())
      this->slaves.emplace_back(mapping.first);
}

ChunkFetcher::~ChunkFetcher()
{
}

/**
 * Start fetcher slaves if they are not running already.
 *
 * @return true if successfully started or already running, false if startup problem occurred.
 */
bool ChunkFetcher::startFetching()
{
   const char* logContext = "ChunkFetcher (start)";
   bool retVal = true; // false if error occurred

   {
      const std::lock_guard<Mutex> lock(chunksListMutex);
      isBad = false;
   }

   for(ChunkFetcherSlaveListIter iter = slaves.begin(); iter != slaves.end(); iter++)
   {
      const std::lock_guard<Mutex> lock(iter->statusMutex);

      if(!iter->isRunning)
      {
         // slave thread not running yet => start it
         iter->resetSelfTerminate();

         try
         {
            iter->start();

            iter->isRunning = true;
         }
         catch (PThreadCreateException& e)
         {
            LogContext(logContext).logErr(std::string("Unable to start thread: ") + e.what());
            retVal = false;
         }
      }
   }

   return retVal;
}

void ChunkFetcher::stopFetching()
{
   for(ChunkFetcherSlaveListIter iter = slaves.begin(); iter != slaves.end(); iter++)
   {
      const std::lock_guard<Mutex> lock(iter->statusMutex);

      if(iter->isRunning)
      {
         iter->selfTerminate();
      }
   }
}

void ChunkFetcher::waitForStopFetching()
{
   for(ChunkFetcherSlaveListIter iter = slaves.begin(); iter != slaves.end(); iter++)
   {
      const std::lock_guard<Mutex> lock(iter->statusMutex);

      chunksListFetchedCondition.broadcast();

      while (iter->isRunning)
      {
         iter->isRunningChangeCond.wait(&(iter->statusMutex));
      }

      chunksList.clear();
   }
}
