#ifndef INCSYNCEDCOUNTERWORK_H
#define INCSYNCEDCOUNTERWORK_H

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>

class IncSyncedCounterWork : public Work
{
   public:
      IncSyncedCounterWork(SynchronizedCounter* counter)
      {
         this->counter = counter;
      }

      virtual ~IncSyncedCounterWork() { };

      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
      {
         LOG_DEBUG("IncSyncedCounterWork", Log_DEBUG,
            "Processing IncSyncedCounterWork");

         // increment counter
         counter->incCount();

         LOG_DEBUG("IncSyncedCounterWork", Log_DEBUG,
            "Processed IncSyncedCounterWork");
      }

   private:
      SynchronizedCounter* counter;
};

#endif /* INCSYNCEDCOUNTERWORK_H */
