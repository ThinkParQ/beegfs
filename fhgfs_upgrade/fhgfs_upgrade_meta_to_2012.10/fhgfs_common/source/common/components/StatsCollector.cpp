#include <common/toolkit/TimeAbs.h>
#include "StatsCollector.h"

StatsCollector::StatsCollector(MultiWorkQueue* workQ, unsigned collectIntervalMS,
   unsigned historyLength) throw(ComponentInitException) :
   PThread("Stats")
{
   log.setContext("Stats");

   this->workQ = workQ;
   this->collectIntervalMS = collectIntervalMS;
   this->historyLength = historyLength;
}

StatsCollector::~StatsCollector()
{
   // nothing to be done here
}


void StatsCollector::run()
{
   try
   {
      registerSignalHandler();

      collectLoop();

      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

void StatsCollector::collectLoop()
{
   while(!waitForSelfTerminateOrder(collectIntervalMS) )
   {
      collectStats();
   }
}


void StatsCollector::collectStats()
{
   HighResolutionStats currentStats;

   SafeMutexLock mutexLock(&mutex);

   // Note: Newer stats in the internal list are pushed at the front side

   // get old stats and reset them
   workQ->getAndResetStats(&currentStats);

   // set current stats time
   currentStats.rawVals.statsTimeMS = TimeAbs().getTimeMS();

   // take care of max history length
   if(statsList.size() == historyLength)
      statsList.pop_back();

   // push new stats to front
   statsList.push_front(currentStats);

   mutexLock.unlock();
}

/**
 * Returns the part of the stats history after lastStatsMS.
 */
void StatsCollector::getStatsSince(uint64_t lastStatsMS, HighResStatsList& outStatsList)
{
   SafeMutexLock mutexLock(&mutex);

   // Note: Newer stats in the internal list are pushed at the front side, but
   // newer stats on the outStatsList are pushed to the back side.

   for(HighResStatsListIter iter = statsList.begin(); iter != statsList.end(); iter++)
   {
      // are the current stats older than requested?
      if(iter->rawVals.statsTimeMS <= lastStatsMS)
         break;

      outStatsList.push_back(*iter);
   }

   mutexLock.unlock();
}
