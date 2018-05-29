#ifndef META_BARRIERWORK_H
#define META_BARRIERWORK_H

#include <common/components/worker/Work.h>
#include <common/threading/Atomics.h>
#include <common/threading/Condition.h>
#include <common/app/log/Logger.h>

/**
 * Work item intended to stop all worker threads temporarily, detect that all are stopped using a
 * barrier, and restarting them using the same barrier.
 * Example:
 * Barrier workerBarrier(numWorkers + 1);
 * <insert instance of BarrierWorkItem(&workerBarrier) into personal queue of numWorkers threads>
 * workerBarrier.wait(); // Wait for all workers to stop
 * <do something while workers are stopped>
 * workerBarrier.wait(); // restart the workers
 */
class BarrierWork : public Work
{
   public:
      BarrierWork(Barrier* barrier) :
         barrier(barrier)
      { }

      virtual ~BarrierWork() { }

      void process(char*, unsigned, char*, unsigned)
      {
         LOG_DBG(WORKQUEUES, DEBUG, "Start blocking.");
         barrier->wait();
         barrier->wait();
         LOG_DBG(WORKQUEUES, DEBUG, "Done.");
      }

   private:
      Barrier* barrier;
};

#endif /* META_BARRIERWORK_H */
