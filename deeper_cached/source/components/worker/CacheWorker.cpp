#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/toolkit/TimeFine.h>

#include "CacheWorker.h"

CacheWorker::CacheWorker(const std::string& workerID, CacheWorkManager* workQueue,
   QueueWorkType workType)
    : PThread(workerID),
      log(workerID),
      terminateWithFullQueue(true),
      bufInLen(WORKER_BUFIN_SIZE),
      bufIn(NULL),
      bufOutLen(WORKER_BUFOUT_SIZE),
      bufOut(NULL),
      workQueue(workQueue),
      workType(workType)
{
}

void CacheWorker::run()
{
   try
   {
      registerSignalHandler();
      initBuffers();


      LOG(GENERAL, DEBUG, "Ready", as("TID", System::getTID() ) );

      while(!getSelfTerminate() || !maySelfTerminateNow() )
      {
         Work* work = waitForWorkByType(workType);

#ifdef BEEGFS_DEBUG_PROFILING
            TimeFine workStartTime;
#endif

         // process the work packet
         work->process(bufIn, bufInLen, bufOut, bufOutLen);

#ifdef BEEGFS_DEBUG_PROFILING
         TimeFine workEndTime;
         int workElapsedMS = workEndTime.elapsedSinceMS(&workStartTime);
         int workElapsedMicro = workEndTime.elapsedSinceMicro(&workStartTime);

         if(workEndTime.elapsedSinceMS(&workStartTime) >= 10)
            LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Work processed. Elapsed ms: " +
               StringTk::intToStr(workElapsedMS) );
         else
            LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Work processed. Elapsed us: " +
               StringTk::intToStr(workElapsedMicro) );
#endif

         // cleanup requires a dynamic cast to handle different types of work
         CacheWork* cacheWork = dynamic_cast<CacheWork*>(work);
         if(cacheWork)
         {
            if(cacheWork->isFinished() )
            {
               SAFE_DELETE(work);
            }
         }
         else
         {
            SAFE_DELETE(work);
         }
      }

      LOG(GENERAL, DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

Work* CacheWorker::waitForWorkByType(QueueWorkType workType)
{
   /* note: we hope the if-conditions below are optimized away when this is called from
      Worker::workLoop(), that's why we have the explicit work type arg in Worker::run() */

   if(workType == QueueWorkType_DIRECT)
      return workQueue->waitForDirectWork();
   else
   if(workType == QueueWorkType_INDIRECT)
      return workQueue->waitForNewWork();
   else // should never happen
      throw WorkerException("Unknown/invalid work type given: " + StringTk::intToStr(workType));
}

/**
 * Note: For delayed buffer allocation during run(), because of NUMA-archs.
 */
void CacheWorker::initBuffers()
{
   if(this->bufInLen)
   {
      void* bufInVoid = NULL;
      int inAllocRes = posix_memalign(&bufInVoid, sysconf(_SC_PAGESIZE), bufInLen);
      IGNORE_UNUSED_VARIABLE(inAllocRes);
      this->bufIn = (char*)bufInVoid;
   }

   if(this->bufOutLen)
   {
      void* bufOutVoid = NULL;
      int outAllocRes = posix_memalign(&bufOutVoid, sysconf(_SC_PAGESIZE), bufOutLen);
      IGNORE_UNUSED_VARIABLE(outAllocRes);
      this->bufOut = (char*)bufOutVoid;
   }
}
