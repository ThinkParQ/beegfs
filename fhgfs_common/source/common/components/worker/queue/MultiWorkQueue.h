#ifndef MULTIWORKQUEUE_H_
#define MULTIWORKQUEUE_H_

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/threading/Mutex.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/toolkit/NamedException.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include "ListWorkContainer.h"
#include "PersonalWorkQueue.h"


#define MULTIWORKQUEUE_DEFAULT_USERID  (~0) // (usually similar to NETMESSAGE_DEFAULT_USERID)


DECLARE_NAMEDEXCEPTION(MultiWorkQueueException, "MultiWorkQueueException")


class MultiWorkQueue; // forward declaration


typedef std::map<uint16_t, MultiWorkQueue*> MultiWorkQueueMap; // maps targetIDs to WorkQueues
typedef MultiWorkQueueMap::iterator MultiWorkQueueMapIter;
typedef MultiWorkQueueMap::const_iterator MultiWorkQueueMapCIter;
typedef MultiWorkQueueMap::value_type MultiWorkQueueMapVal;



/**
 * Note: We also use these numbers as indices in the MultiWorkQueue::workListVec, so carefully
 * check all related cases when you add/change something here.
 */
enum QueueWorkType
{
   QueueWorkType_DIRECT=0,
   QueueWorkType_INDIRECT,

   QueueWorkType_FINAL_DONTUSE // the final element as terminating vector index
};


class MultiWorkQueue
{
   private:
      // type definitions
      typedef std::vector<AbstractWorkContainer*> WorkListVec;
      typedef WorkListVec::iterator WorkListVecIter;
      typedef WorkListVec::const_iterator WorkListVecCIter;


   public:
      MultiWorkQueue();
      ~MultiWorkQueue();

      Work* waitForDirectWork(HighResolutionStats& newStats, PersonalWorkQueue* personalWorkQueue);
      Work* waitForAnyWork(HighResolutionStats& newStats, PersonalWorkQueue* personalWorkQueue);

      void incNumWorkers();

      void setIndirectWorkList(AbstractWorkContainer* newWorkList);

      void getStatsAsStr(std::string& outIndirectQueueStats, std::string& outDirectQueueStats,
         std::string& outBusyStats);


   private:
      AbstractWorkContainer* directWorkList;
      AbstractWorkContainer* indirectWorkList;

      Mutex mutex;
      Condition newDirectWorkCond; // direct workers wait only on this condition
      Condition newWorkCond; // for any type of work (indirect workers wait on this condition)

      size_t numPendingWorks; // length of direct+indirect list (not incl personal list)
      unsigned lastWorkListVecIdx; // toggles indirect workers types of work (% queue types)
      WorkListVec workListVec; // used to toggle next work type with nextWorkType as index

      HighResolutionStats stats;

   public:
      void addDirectWork(Work* work, unsigned userID = MULTIWORKQUEUE_DEFAULT_USERID)
      {
         SafeMutexLock mutexLock(&mutex);

         directWorkList->addWork(work, userID);

         numPendingWorks++;

         newWorkCond.signal();
         newDirectWorkCond.signal();

         mutexLock.unlock();
      }

      void addIndirectWork(Work* work, unsigned userID = MULTIWORKQUEUE_DEFAULT_USERID)
      {
         SafeMutexLock mutexLock(&mutex);

         indirectWorkList->addWork(work, userID);

         numPendingWorks++;

         newWorkCond.signal();

         mutexLock.unlock();
      }

      void addPersonalWork(Work* work, PersonalWorkQueue* personalQ)
      {
         /* note: this is in the here (instead of the PersonalWorkQueue) because the MultiWorkQueue
            mutex also syncs the personal queue. */

         SafeMutexLock mutexLock(&mutex);

         personalQ->addWork(work);

         // note: we do not increase numPendingWorks here (it is only for the other queues)

         // we assume this method is rarely used, so we just wake up all wokers (inefficient)
         newDirectWorkCond.broadcast();
         newWorkCond.broadcast();

         mutexLock.unlock();
      }
      
      size_t getDirectWorkListSize()
      {
         SafeMutexLock mutexLock(&mutex);

         size_t retVal = directWorkList->getSize();

         mutexLock.unlock();

         return retVal;
      }

      size_t getIndirectWorkListSize()
      {
         SafeMutexLock mutexLock(&mutex);

         size_t retVal = indirectWorkList->getSize();

         mutexLock.unlock(); 

         return retVal;
      }
      
      bool getIsPersonalQueueEmpty(PersonalWorkQueue* personalQ)
      {
         SafeMutexLock mutexLock(&mutex);

         bool retVal = personalQ->getIsWorkListEmpty();

         mutexLock.unlock();

         return retVal;
      }

      size_t getNumPendingWorks()
      {
         SafeMutexLock mutexLock(&mutex);

         size_t retVal = numPendingWorks;

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
         outStats->rawVals.queuedRequests = numPendingWorks;

         /* note: we only reset incremental stats vals, because otherwise we would lose info
            like number of busyWorkers */
         HighResolutionStatsTk::resetIncStats(&stats);

         mutexLock.unlock();
      }


};

#endif /*MULTIWORKQUEUE_H_*/
