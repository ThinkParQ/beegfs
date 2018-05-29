#ifndef CACHEWORKER_H_
#define CACHEWORKER_H_

#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/components/worker/queue/PersonalWorkQueue.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>
#include <components/worker/queue/CacheWorkManager.h>


#define WORKER_BUFIN_SIZE     (1024*1024*4)
#define WORKER_BUFOUT_SIZE    WORKER_BUFIN_SIZE


DECLARE_NAMEDEXCEPTION(WorkerException, "WorkerException")



class CacheWorker : public PThread
{
   public:
      CacheWorker(const std::string& workerID, CacheWorkManager* workQueue, QueueWorkType workType);

      virtual ~CacheWorker()
      {
         SAFE_FREE(bufIn);
         SAFE_FREE(bufOut);
      }


   private:
      LogContext log;
      bool terminateWithFullQueue; // allow self-termination when queue not empty (see setter nodes)

      size_t bufInLen;
      char* bufIn;
      size_t bufOutLen;
      char* bufOut;

      CacheWorkManager* workQueue;
      QueueWorkType workType;


      void initBuffers();
      virtual void run();

      Work* waitForWorkByType(QueueWorkType workType);


      // inliners
      bool maySelfTerminateNow()
      {
         if(terminateWithFullQueue || (!workQueue->getSize() ) )
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

      CacheWorkManager* getWorkQueue() const
      {
         return this->workQueue;
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

#endif /*CACHEWORKER_H_*/
