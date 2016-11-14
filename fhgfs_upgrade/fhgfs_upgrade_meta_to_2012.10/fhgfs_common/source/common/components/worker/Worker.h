#ifndef WORKER_H_
#define WORKER_H_

#include <common/app/log/LogContext.h>
#include <common/app/AbstractApp.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>
#include "MultiWorkQueue.h"


#define WORKER_BUFIN_SIZE     (1024*1024*4)
#define WORKER_BUFOUT_SIZE    WORKER_BUFIN_SIZE


class Worker : public PThread
{
   public:
      Worker(std::string workerID, MultiWorkQueue* workQueue, QueueWorkType workType)
         throw(ComponentInitException);
      
      virtual ~Worker()
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

      MultiWorkQueue* workQueue;
      QueueWorkType workType;
      
      HighResolutionStats stats;

      
      virtual void run();
      
      void workLoopAnyWork();
      void workLoopDirectWork();
      
      void initBuffers();
      
      // inliners
      bool maySelfTerminateNow()
      {
         if(terminateWithFullQueue ||
            (!workQueue->getDirectWorkListSize() && !workQueue->getIndirectWorkListSize() ) )
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
