#ifndef BUDDYRESYNCERGATHERSLAVE_H_
#define BUDDYRESYNCERGATHERSLAVE_H_

#include <common/app/log/LogContext.h>
#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>

#include <ftw.h>

#define GATHERSLAVEQUEUE_MAXSIZE 5000

class BuddyResyncerGatherSlaveWorkQueue
{
   /*
    * This is more or less just a small class for convenience, that is tightly coupled to
    * BuddyResyncerGatherSlave and BuddyResyncerJob
    */
   public:
      BuddyResyncerGatherSlaveWorkQueue(): gatherSlavesWorkQueueLen(0) { }

   private:
      StringList paths;
      size_t gatherSlavesWorkQueueLen; // used to avoid constant calling of size() method of list
      Mutex mutex;
      Condition pathAddedCond;
      Condition pathFetchedCond;

   public:
      void add(std::string& path, PThread* caller)
      {
         unsigned waitTimeoutMS = 3000;

         SafeMutexLock mutexLock(&mutex);

         while (gatherSlavesWorkQueueLen > GATHERSLAVEQUEUE_MAXSIZE)
         {
            if((caller) && (unlikely(caller->getSelfTerminate())))
               break;
            pathFetchedCond.timedwait(&mutex, waitTimeoutMS);
         }

         paths.push_back(path);
         gatherSlavesWorkQueueLen++;
         pathAddedCond.signal();

         mutexLock.unlock();
      }

      std::string fetch(PThread* caller)
      {
         unsigned waitTimeoutMS = 3000;

         std::string retVal;

         SafeMutexLock mutexLock(&mutex);

         while (paths.empty())
         {
            if((caller) && (unlikely(caller->getSelfTerminate())))
            {
               retVal = "";
               goto unlock_and_return;
            }

            pathAddedCond.timedwait(&mutex, waitTimeoutMS);
         }

         retVal = paths.front();
         paths.pop_front();
         gatherSlavesWorkQueueLen--;
         pathFetchedCond.signal();

         unlock_and_return:
         mutexLock.unlock();

         return retVal;
      }

      bool queueEmpty()
      {
         bool retVal;

         SafeMutexLock mutexLock(&mutex);

         retVal = (gatherSlavesWorkQueueLen == 0);

         mutexLock.unlock();

         return retVal;
      }

      void clear()
      {
         SafeMutexLock mutexLock(&mutex);

         paths.clear();
         gatherSlavesWorkQueueLen = 0;

         mutexLock.unlock();
      }
};

class BuddyResyncerGatherSlave : public PThread
{
   friend class BuddyResyncer; // (to grant access to internal mutex)
   friend class BuddyResyncJob; // (to grant access to internal mutex)

   public:
      BuddyResyncerGatherSlave(uint16_t targetID, ChunkSyncCandidateStore* syncCandidates,
         BuddyResyncerGatherSlaveWorkQueue* workQueue, uint8_t slaveID);
      virtual ~BuddyResyncerGatherSlave();

      void workLoop();

   private:
      AtomicSizeT onlyTerminateIfIdle; // atomic quasi-bool

      Mutex statusMutex; // protects isRunning
      Condition isRunningChangeCond;

      uint16_t targetID;

      AtomicUInt64 numChunksDiscovered;
      AtomicUInt64 numChunksMatched;

      AtomicUInt64 numDirsDiscovered;
      AtomicUInt64 numDirsMatched;

      bool isRunning; // true if an instance of this component is currently running

      ChunkSyncCandidateStore* syncCandidates;
      BuddyResyncerGatherSlaveWorkQueue* workQueue;

      // nftw() callback needs access the slave threads
      static Mutex staticGatherSlavesMutex;
      static std::map<std::string, BuddyResyncerGatherSlave*> staticGatherSlaves;

      virtual void run();

      static int handleDiscoveredEntry(const char* path, const struct stat* statBuf,
         int ftwEntryType, struct FTW* ftwBuf);

   public:
      // getters & setters
      bool getIsRunning()
      {
         SafeMutexLock safeLock(&statusMutex);

         bool retVal = this->isRunning;

         safeLock.unlock();

         return retVal;
      }

      uint16_t getTargetID()
      {
         return targetID;
      }

      void getCounters(uint64_t& outNumChunksDiscovered, uint64_t& outNumChunksMatched,
         uint64_t& outNumDirsDiscovered, uint64_t& outNumDirsMatched)
      {
         outNumChunksDiscovered = numChunksDiscovered.read();
         outNumChunksMatched = numChunksMatched.read();
         outNumDirsDiscovered = numDirsDiscovered.read();
         outNumDirsMatched = numDirsMatched.read();
      }

      void setOnlyTerminateIfIdle(bool value)
      {
         if (value)
            onlyTerminateIfIdle.set(1);
         else
            onlyTerminateIfIdle.setZero();
      }

      bool getOnlyTerminateIfIdle()
      {
         if (onlyTerminateIfIdle.read() == 0)
            return false;
         else
            return true;
      }

   private:
      // getters & setters

      void setIsRunning(bool isRunning)
      {
         SafeMutexLock safeLock(&statusMutex);

         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();

         safeLock.unlock();
      }

      bool getSelfTerminateNotIdle()
      {
         return ( (getSelfTerminate() && (!getOnlyTerminateIfIdle())) );
      }
};

typedef std::vector<BuddyResyncerGatherSlave*> BuddyResyncerGatherSlaveVec;
typedef BuddyResyncerGatherSlaveVec::iterator BuddyResyncerGatherSlaveVecIter;

#endif /* BUDDYRESYNCERGATHERSLAVE_H_ */
