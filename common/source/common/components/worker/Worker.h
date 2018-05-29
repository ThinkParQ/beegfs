#ifndef WORKER_H_
#define WORKER_H_

#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/components/worker/queue/PersonalWorkQueue.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>


#define WORKER_BUFIN_SIZE     (1024*1024*4)
#define WORKER_BUFOUT_SIZE    WORKER_BUFIN_SIZE


DECLARE_NAMEDEXCEPTION(WorkerException, "WorkerException")



class Worker : public PThread
{
   public:
      Worker(const std::string& workerID, MultiWorkQueue* workQueue, QueueWorkType workType);

      virtual ~Worker()
      {
         SAFE_FREE(bufIn);
         SAFE_FREE(bufOut);
         SAFE_DELETE(personalWorkQueue);
      }


   private:
      LogContext log;
      bool terminateWithFullQueue; // allow self-termination when queue not empty (see setter nodes)

      size_t bufInLen;
      char* bufIn;
      size_t bufOutLen;
      char* bufOut;

      MultiWorkQueue* workQueue;
      QueueWorkType workType;

      PersonalWorkQueue* personalWorkQueue;

      HighResolutionStats stats;


      virtual void run();

      void workLoop(QueueWorkType workType);
      Work* waitForWorkByType(HighResolutionStats& newStats, PersonalWorkQueue* personalWorkQueue,
         QueueWorkType workType);

      void initBuffers();

      // inliners
      bool maySelfTerminateNow()
      {
         if(terminateWithFullQueue ||
            (!workQueue->getNumPendingWorks() &&
               workQueue->getIsPersonalQueueEmpty(personalWorkQueue) ) )
            return true;

         return false;
      }


   public:
      // setters & getters

      /**
       * Note: Do not use this after the run method of this component has been called!
       */
      void setBufLens(size_t bufInLen, size_t bufOutLen)
      {
         this->bufInLen = bufInLen;
         this->bufOutLen = bufOutLen;
      }

      MultiWorkQueue* getWorkQueue() const
      {
         return this->workQueue;
      }

      /**
       * Note: Don't add anything to this queue directly, do it only via
       * MultiWorkQueue->addPersonalWork().
       */
      PersonalWorkQueue* getPersonalWorkQueue()
      {
         return this->personalWorkQueue;
      }

      /**
       * WARNING: This will only work if there is only a single worker attached to a queue.
       * Otherwise the queue would need a getWorkAndDontWait() method that is used during the
       * termination phase of the worker, because the queue might become empty before the worker
       * calls waitForWork() after the maySelfTerminateNow check.
       */
      void disableTerminationWithFullQueue()
      {
         this->terminateWithFullQueue = false;
      }
};

#endif /*WORKER_H_*/
