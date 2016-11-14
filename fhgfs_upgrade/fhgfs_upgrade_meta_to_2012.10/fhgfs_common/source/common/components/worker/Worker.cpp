#include "Worker.h"

Worker::Worker(std::string workerID, MultiWorkQueue* workQueue, QueueWorkType workType)
   throw(ComponentInitException) :
   PThread(workerID)
{
   log.setContext(workerID);

   this->workQueue = workQueue;
   this->workType = workType;

   HighResolutionStatsTk::resetStats(&this->stats);

   this->bufInLen = WORKER_BUFIN_SIZE;
   this->bufIn = NULL;
   this->bufOutLen = WORKER_BUFOUT_SIZE;
   this->bufOut = NULL;

   this->terminateWithFullQueue = true;
}

void Worker::run()
{
   try
   {
      registerSignalHandler();

      initBuffers();

      if(workType == QueueWorkType_DIRECT)
         workLoopDirectWork();
      else
         workLoopAnyWork();

      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

}

void Worker::workLoopDirectWork()
{
   log.log(4, std::string("Ready (TID: ") + StringTk::uint64ToStr(System::getTID() ) + ")");

   workQueue->incNumWorkers(); // add this worker to queue stats

   while(!getSelfTerminate() || !maySelfTerminateNow() )
   {
      //log.log(4, "Waiting for work...");

      Work* work = workQueue->waitForDirectWork(stats);

      //log.log(4, "Got work");

      #ifdef FHGFS_DEBUG_PROFILING
         Time workStartTime;
      #endif

      HighResolutionStatsTk::resetStats(&stats); // prepare stats

      // process the work packet
      work->process(bufIn, bufInLen, bufOut, bufOutLen);

      // update stats
      stats.incVals.workRequests = 1;
      HighResolutionStatsTk::addHighResIncStats(*work->getHighResolutionStats(), stats);

      #ifdef FHGFS_DEBUG_PROFILING
         Time workEndTime;
         int workElapsedMS = workEndTime.elapsedSinceMS(&workStartTime);
         int workElapsedMicro = workEndTime.elapsedSinceMicro(&workStartTime);

         if(workEndTime.elapsedSinceMS(&workStartTime) )
            LOG_DEBUG("Worker::workLoop", Log_DEBUG, "Work processed. Elapsed ms: " +
               StringTk::intToStr(workElapsedMS) );
         else
            LOG_DEBUG("Worker::workLoop", Log_DEBUG, "Work processed. Elapsed us: " +
               StringTk::intToStr(workElapsedMicro) );
      #endif

      // cleanup
      delete(work);
   }
}

void Worker::workLoopAnyWork()
{
   log.log(4, std::string("Ready (TID: ") + StringTk::uint64ToStr(System::getTID() ) + ")");

   workQueue->incNumWorkers(); // add this worker to queue stats

   while(!getSelfTerminate() || !maySelfTerminateNow() )
   {
      //log.log(4, "Waiting for work...");

      Work* work = workQueue->waitForAnyWork(stats);

      //log.log(4, "Got work");

      #ifdef FHGFS_DEBUG_PROFILING
         Time workStartTime;
      #endif

      HighResolutionStatsTk::resetStats(&stats); // prepare stats

      // process the work packet
      work->process(bufIn, bufInLen, bufOut, bufOutLen);

      // update stats
      stats.incVals.workRequests = 1;
      HighResolutionStatsTk::addHighResIncStats(*work->getHighResolutionStats(), stats);

      #ifdef FHGFS_DEBUG_PROFILING
      Time workEndTime;
      int workElapsedMS = workEndTime.elapsedSinceMS(&workStartTime);
      int workElapsedMicro = workEndTime.elapsedSinceMicro(&workStartTime);

      if(workEndTime.elapsedSinceMS(&workStartTime) )
         LOG_DEBUG("Worker::workLoop", Log_DEBUG, "Work processed. Elapsed ms: " +
            StringTk::intToStr(workElapsedMS) );
      else
         LOG_DEBUG("Worker::workLoop", Log_DEBUG, "Work processed. Elapsed us: " +
            StringTk::intToStr(workElapsedMicro) );
      #endif

      // cleanup
      delete(work);
   }
}

/**
 * Note: For delayed buffer allocation during run(), because of NUMA-archs.
 */
void Worker::initBuffers()
{
   if(this->bufInLen)
   {
      void* bufInVoid;
      int inAllocRes = posix_memalign(&bufInVoid, sysconf(_SC_PAGESIZE), bufInLen);
      IGNORE_UNUSED_VARIABLE(inAllocRes);
      this->bufIn = (char*)bufInVoid;
   }

   if(this->bufOutLen)
   {
      void* bufOutVoid;
      int outAllocRes = posix_memalign(&bufOutVoid, sysconf(_SC_PAGESIZE), bufOutLen);
      IGNORE_UNUSED_VARIABLE(outAllocRes);
      this->bufOut = (char*)bufOutVoid;
   }
}
