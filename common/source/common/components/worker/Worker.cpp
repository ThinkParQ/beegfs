#include <common/toolkit/TimeFine.h>
#include "Worker.h"

Worker::Worker(const std::string& workerID, MultiWorkQueue* workQueue, QueueWorkType workType)
    : PThread(workerID),
      log(workerID),
      terminateWithFullQueue(true),
      bufInLen(WORKER_BUFIN_SIZE),
      bufIn(NULL),
      bufOutLen(WORKER_BUFOUT_SIZE),
      bufOut(NULL),
      workQueue(workQueue),
      workType(workType),
      personalWorkQueue(new PersonalWorkQueue() )
{
   HighResolutionStatsTk::resetStats(&this->stats);
}

void Worker::run()
{
   try
   {
      registerSignalHandler();

      initBuffers();

      /* note: we're not directly calling workLoop(workType) below, because:
         1) we want to check that the given workType value is really valid.
         2) we hope that the compiler can use this explicit check and value passing to optimize away
            the if-condition inside waitForWorkByType (so that a worker does not need to check its
            worktype for every incoming work). */

      if(workType == QueueWorkType_DIRECT)
         workLoop(QueueWorkType_DIRECT);
      else
      if(workType == QueueWorkType_INDIRECT)
         workLoop(QueueWorkType_INDIRECT);
      else
         throw ComponentInitException(
            "Unknown/invalid work type given: " + StringTk::intToStr(workType) );

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

}

void Worker::workLoop(QueueWorkType workType)
{
   LOG(WORKQUEUES, DEBUG, "Ready", ("TID", System::getTID()), workType);

   workQueue->incNumWorkers(); // add this worker to queue stats

   while(!getSelfTerminate() || !maySelfTerminateNow() )
   {
      Work* work = waitForWorkByType(stats, personalWorkQueue, workType);

#ifdef BEEGFS_DEBUG_PROFILING
      TimeFine workStartTime;
#endif

      HighResolutionStatsTk::resetStats(&stats); // prepare stats

      // process the work packet
      work->process(bufIn, bufInLen, bufOut, bufOutLen);

      // update stats
      stats.incVals.workRequests = 1;
      HighResolutionStatsTk::addHighResIncStats(*work->getHighResolutionStats(), stats);

#ifdef BEEGFS_DEBUG_PROFILING
      TimeFine workEndTime;
      const auto workElapsedMS = workEndTime.elapsedSinceMS(&workStartTime);
      const auto workLatencyMS = workEndTime.elapsedSinceMS(work->getAgeTime());

      if (workElapsedMS >= 10)
      {
         if (workLatencyMS >= 10)
            LOG(WORKQUEUES, DEBUG, "Work processed.",
                  ("Elapsed ms", workElapsedMS), ("Total latency (ms)", workLatencyMS));
         else
            LOG(WORKQUEUES, DEBUG, "Work processed.", ("Elapsed ms", workElapsedMS),
                  ("Total latency (us)", workEndTime.elapsedSinceMicro(work->getAgeTime())));
      }
      else
      {
         if (workLatencyMS >= 10)
         {
            LOG(WORKQUEUES, DEBUG, "Work processed.",
                  ("Elapsed us", workEndTime.elapsedSinceMicro(&workStartTime)),
                  ("Total latency (ms)", workEndTime.elapsedSinceMS(work->getAgeTime())));

         }
         else
         {
            LOG(WORKQUEUES, DEBUG, "Work processed.",
                  ("Elapsed us", workEndTime.elapsedSinceMicro(&workStartTime)),
                  ("Total latency (us)", workEndTime.elapsedSinceMicro(work->getAgeTime())));
         }
      }
#endif

      // cleanup
      delete(work);
   }
}

Work* Worker::waitForWorkByType(HighResolutionStats& newStats, PersonalWorkQueue* personalWorkQueue,
   QueueWorkType workType)
{
   /* note: we hope the if-conditions below are optimized away when this is called from
      Worker::workLoop(), that's why we have the explicit work type arg in Worker::run() */

   if(workType == QueueWorkType_DIRECT)
      return workQueue->waitForDirectWork(newStats, personalWorkQueue);
   else
   if(workType == QueueWorkType_INDIRECT)
      return workQueue->waitForAnyWork(newStats, personalWorkQueue);
   else // should never happen
      throw WorkerException("Unknown/invalid work type given: " + StringTk::intToStr(workType));
}


/**
 * Note: For delayed buffer allocation during run(), because of NUMA-archs.
 */
void Worker::initBuffers()
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
