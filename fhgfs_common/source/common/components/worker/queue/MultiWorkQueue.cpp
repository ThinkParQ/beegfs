#include "MultiWorkQueue.h"
#include "PersonalWorkQueue.h"


MultiWorkQueue::MultiWorkQueue()
{
   numPendingWorks = 0;
   lastWorkListVecIdx = 0;

   directWorkList = new ListWorkContainer();
   indirectWorkList = new ListWorkContainer();

   // we use QueueWorkType_... as vec index, so order must be same as in QueueWorkType
   workListVec.push_back(directWorkList);
   workListVec.push_back(indirectWorkList);

   HighResolutionStatsTk::resetStats(&stats);
}

MultiWorkQueue::~MultiWorkQueue()
{
   delete(directWorkList);
   delete(indirectWorkList);
}

/**
 * Code base of waitForDirectWork.
 */
Work* MultiWorkQueue::waitForDirectWork(HighResolutionStats& newStats,
   PersonalWorkQueue* personalWorkQueue)
{
   Work* work;

   SafeMutexLock mutexLock(&mutex); // L O C K

   HighResolutionStatsTk::addHighResIncStats(newStats, stats);
   stats.rawVals.busyWorkers--;

   while(directWorkList->getIsEmpty() && likely(personalWorkQueue->getIsWorkListEmpty() ) )
      newDirectWorkCond.wait(&mutex);

   stats.rawVals.busyWorkers++;

   // personal is always first
   if(unlikely(!personalWorkQueue->getIsWorkListEmpty() ) )
   { // we got something in our personal queue
      work = personalWorkQueue->getAndPopFirstWork();
   }
   else
   {
      work = directWorkList->getAndPopNextWork();

      numPendingWorks--;
   }

   mutexLock.unlock(); // U N L O C K

   return work;
}

/**
 * Wait for work on any of the avialable queues. This is for indirect (non-specialized) workers.
 *
 * @param newStats the updated stats from processing of the last work package.
 * @param personalWorkQueue the personal queue of the worker thread which called this method.
 */
Work* MultiWorkQueue::waitForAnyWork(HighResolutionStats& newStats,
   PersonalWorkQueue* personalWorkQueue)
{
   Work* work;

   SafeMutexLock mutexLock(&mutex); // L O C K

   HighResolutionStatsTk::addHighResIncStats(newStats, stats);
   stats.rawVals.busyWorkers--;

   while(!numPendingWorks && likely(personalWorkQueue->getIsWorkListEmpty() ) )
   { // no work available right now
      newWorkCond.wait(&mutex);
   }

   stats.rawVals.busyWorkers++;

   // sanity check: ensure numPendingWorks and actual number of works are equal
   #ifdef BEEGFS_DEBUG
      size_t numQueuedWorks = 0;

      for(unsigned i=0; i < QueueWorkType_FINAL_DONTUSE; i++)
         numQueuedWorks += workListVec[i]->getSize();

      if(unlikely(numQueuedWorks != numPendingWorks) )
         throw MultiWorkQueueException("numQueuedWorks != numPendingWorks: " +
            StringTk::uint64ToStr(numQueuedWorks) + "!=" + StringTk::uint64ToStr(numPendingWorks) );
   #endif // BEEGFS_DEBUG


   // personal is always first
   if(unlikely(!personalWorkQueue->getIsWorkListEmpty() ) )
   { // we got something in our personal queue
      work = personalWorkQueue->getAndPopFirstWork();
   }
   else
   {
      // walk over all available queues
      // (note: lastWorkListVecIdx ensures that all queue are checked in a fair way)

      for(unsigned i=0; i < QueueWorkType_FINAL_DONTUSE; i++)
      {
         // switch to next work queue
         lastWorkListVecIdx++;
         lastWorkListVecIdx = lastWorkListVecIdx % QueueWorkType_FINAL_DONTUSE;

         AbstractWorkContainer* currentWorkList = workListVec[lastWorkListVecIdx];

         if(!currentWorkList->getIsEmpty() )
         { // this queue contains work for us
            work = currentWorkList->getAndPopNextWork();

            numPendingWorks--;

            goto unlock_and_exit;
         }
      }

      // we should never get here: all queues are empty

      throw MultiWorkQueueException("Unexpected in " + std::string(__func__) + ": "
         "All queues are empty. "
         "numPendingWorks: " + StringTk::uint64ToStr(numPendingWorks) );
   }


unlock_and_exit:
   mutexLock.unlock(); // U N L O C K

   return work;
}

/**
 * Adds a new worker thread to internal stats counters to provide correct number of idle/busy
 * workers.
 *
 * This must be called exactly once by each worker before the worker calls waitFor...Work().
 */
void MultiWorkQueue::incNumWorkers()
{
   SafeMutexLock mutexLock(&mutex);

   /* note: we increase number of busy workers here, because this value will be decreased
      by 1 when the worker calls waitFor...Work(). */
   stats.rawVals.busyWorkers++;

   mutexLock.unlock();
}

/**
 * Deletes the old list and replaces it with the new given one.
 *
 * Note: Unlocked, because this is intended to be called during queue preparation.
 *
 * @param newWorkList will be owned by this class, so do no longer access it directly and don't
 * delete it.
 */
void MultiWorkQueue::setIndirectWorkList(AbstractWorkContainer* newWorkList)
{
   #ifdef BEEGFS_DEBUG
      // sanity check
      if(!indirectWorkList->getIsEmpty() )
         throw MultiWorkQueueException("Unexpected in " + std::string(__func__) + ": "
            "Queue to be replaced is not empty.");
   #endif // BEEGFS_DEBUG


   delete(indirectWorkList);

   indirectWorkList = newWorkList;

   workListVec[QueueWorkType_INDIRECT] = indirectWorkList;
}

/**
 * Note: Holds lock while generating stats strings => slow => use carefully
 */
void MultiWorkQueue::getStatsAsStr(std::string& outIndirectQueueStats,
   std::string& outDirectQueueStats, std::string& outBusyStats)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   // get queue stats
   indirectWorkList->getStatsAsStr(outIndirectQueueStats);
   directWorkList->getStatsAsStr(outDirectQueueStats);

   // number of busy workers
   std::ostringstream busyStream;

   busyStream << "* Busy workers:  " << StringTk::uintToStr(stats.rawVals.busyWorkers) << std::endl;
   busyStream << "* Work Requests: " << StringTk::uintToStr(stats.incVals.workRequests) << " "
      "(reset every second)" << std::endl;
   busyStream << "* Bytes read:    " << StringTk::uintToStr(stats.incVals.diskReadBytes) << " "
      "(reset every second)" << std::endl;
   busyStream << "* Bytes written: " << StringTk::uintToStr(stats.incVals.diskWriteBytes) << " "
      "(reset every second)" << std::endl;

   outBusyStats = busyStream.str();

   mutexLock.unlock(); // U N L O C K
}
