#ifndef MULTIWORKQUEUE_H_
#define MULTIWORKQUEUE_H_

#include <common/components/worker/Work.h>
#include <common/threading/Mutex.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>

enum QueueWorkType
   {QueueWorkType_DIRECT=0, QueueWorkType_INDIRECT=1};

class MultiWorkQueue
{
   public:
      MultiWorkQueue()
      {
         nextWorkType = QueueWorkType_DIRECT;
         HighResolutionStatsTk::resetStats(&stats);
      }

      ~MultiWorkQueue()
      {
         for(WorkListIter iter = directWorkList.begin(); iter != directWorkList.end(); iter++)
            delete(*iter);
         for(WorkListIter iter = indirectWorkList.begin(); iter != indirectWorkList.end(); iter++)
            delete(*iter);
      }


   private:
      WorkList directWorkList;
      WorkList indirectWorkList;

      Mutex mutex;
      Condition newDirectWorkCond;
      Condition newWorkCond; // for any type of work

      enum QueueWorkType nextWorkType; // toggles between types of work (for indirect workers)

      HighResolutionStats stats;


   public:
      Work* waitForDirectWork(HighResolutionStats& newStats)
      {
         SafeMutexLock mutexLock(&mutex);

         HighResolutionStatsTk::addHighResIncStats(newStats, stats);
         stats.rawVals.busyWorkers--;

         while(directWorkList.empty() )
            newDirectWorkCond.wait(&mutex);

         stats.rawVals.busyWorkers++;

         Work* work = *directWorkList.begin();
         directWorkList.pop_front();

         mutexLock.unlock();

         return work;
      }

      Work* waitForAnyWork(HighResolutionStats& newStats)
      {
         Work* work;

         SafeMutexLock mutexLock(&mutex);

         HighResolutionStatsTk::addHighResIncStats(newStats, stats);
         stats.rawVals.busyWorkers--;

         while(indirectWorkList.empty() && directWorkList.empty() )
            newWorkCond.wait(&mutex);

         stats.rawVals.busyWorkers++;

         if( ( (nextWorkType == QueueWorkType_INDIRECT) && !indirectWorkList.empty() ) ||
            directWorkList.empty() )
         { // we got indirect work to do
            nextWorkType = QueueWorkType_DIRECT;
            work = *indirectWorkList.begin();
            indirectWorkList.pop_front();
         }
         else
         { // we got direct work to do
            nextWorkType = QueueWorkType_INDIRECT;
            work = *directWorkList.begin();
            directWorkList.pop_front();
         }

         mutexLock.unlock();

         return work;
      }

      void addDirectWork(Work* work)
      {
         SafeMutexLock mutexLock(&mutex);

         directWorkList.push_back(work);

         newWorkCond.signal();
         newDirectWorkCond.signal();

         mutexLock.unlock();
      }

      void addIndirectWork(Work* work)
      {
         SafeMutexLock mutexLock(&mutex);

         indirectWorkList.push_back(work);

         newWorkCond.signal();

         mutexLock.unlock();
      }
      
      size_t getDirectWorkListSize()
      {
         SafeMutexLock mutexLock(&mutex);

         size_t retVal = directWorkList.size();

         mutexLock.unlock();

         return retVal;
      }
      
      size_t getIndirectWorkListSize()
      {
         SafeMutexLock mutexLock(&mutex);

         size_t retVal = indirectWorkList.size();

         mutexLock.unlock(); 

         return retVal;
      }
      
      /**
       * Returns current stats and _resets_ them.
       */
      void getAndResetStats(HighResolutionStats* outStats)
      {
         SafeMutexLock mutexLock(&mutex);

         *outStats = stats;
         outStats->rawVals.queuedRequests = directWorkList.size() + indirectWorkList.size();

         /* note: we only reset incremental stats vals, because otherwise we would lose info
            like number of busyWorkers */
         HighResolutionStatsTk::resetIncStats(&stats);

         mutexLock.unlock();
      }

      /**
       * Adds a new worker thread to internal stats counters to provide correct number of idle/busy
       * workers.
       *
       * This must be called exactly once by each worker before the worker calls waitFor...Work().
       */
      void incNumWorkers()
      {
         SafeMutexLock mutexLock(&mutex);

         /* note: we increase number of busy workers here, because this value will be decreased
            by 1 when the worker calls waitFor...Work(). */
         stats.rawVals.busyWorkers++;

         mutexLock.unlock();
      }


};

#endif /*MULTIWORKQUEUE_H_*/
