#ifndef STATSCOLLECTOR_H_
#define STATSCOLLECTOR_H_

#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>
#include <common/net/message/NetMessage.h>
#include <common/Common.h>


#define STATSCOLLECTOR_COLLECT_INTERVAL_MS   1000
#define STATSCOLLECTOR_HISTORY_LENGTH        60


class StatsCollector : public PThread
{
   public:
      StatsCollector(MultiWorkQueue* workQ, unsigned collectIntervalMS, unsigned historyLength);
      virtual ~StatsCollector();


      void getStatsSince(uint64_t lastStatsMS, HighResStatsList& outStatsList);


   protected:
      LogContext log;

      Mutex mutex;

      HighResStatsList statsList;

      MultiWorkQueue* workQ; // might be NULL in derived classes with own collectStats()
      unsigned collectIntervalMS;
      unsigned historyLength;

      virtual void collectStats();


   private:
      virtual void run();
      void collectLoop();



   public:
      // getters & setters
};



#endif /* STATSCOLLECTOR_H_ */
