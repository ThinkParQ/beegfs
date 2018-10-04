#include <app/App.h>
#include <program/Program.h>
#include "StorageStatsCollector.h"

/**
 * Note: Other than the common StatsCollector::collectStats(), this method can handle multiple work
 * queues.
 */
void StorageStatsCollector::collectStats()
{
   App* app = Program::getApp();
   MultiWorkQueueMap* workQueueMap = app->getWorkQueueMap();

   HighResolutionStats newStats;

   const std::lock_guard<Mutex> lock(mutex);

   // get stats from first queue as basis

   MultiWorkQueueMapIter iter = workQueueMap->begin();

   iter->second->getAndResetStats(&newStats);

   // add the stat values from following queues

   iter++;

   for( ; iter != workQueueMap->end(); iter++)
   {
      HighResolutionStats currentStats;

      iter->second->getAndResetStats(&currentStats);

      HighResolutionStatsTk::addHighResRawStats(currentStats, newStats);
      HighResolutionStatsTk::addHighResIncStats(currentStats, newStats);
   }

   // set current stats time
   newStats.rawVals.statsTimeMS = TimeAbs().getTimeMS();

   // take care of max history length
   if(statsList.size() == historyLength)
      statsList.pop_back();

   // push new stats to front
   statsList.push_front(newStats);
}
