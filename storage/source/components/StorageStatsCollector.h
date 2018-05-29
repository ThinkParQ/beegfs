#ifndef STORAGESTATSCOLLECTOR_H_
#define STORAGESTATSCOLLECTOR_H_

#include <common/components/StatsCollector.h>

/**
 * Common StatsCollector cannot handle multiple work queues, so this derived class overrides
 * the collectStats() method to handle multiple work queues.
 */
class StorageStatsCollector : public StatsCollector
{
   public:
      StorageStatsCollector(unsigned collectIntervalMS, unsigned historyLength):
         StatsCollector(NULL, collectIntervalMS, historyLength)
      {
         // nothing to be done here
      }

      virtual ~StorageStatsCollector() {}


   protected:
      virtual void collectStats();

};

#endif /* STORAGESTATSCOLLECTOR_H_ */
