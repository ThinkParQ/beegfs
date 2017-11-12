#ifndef BUDDYRESYNCERFILESYNCSLAVE_H_
#define BUDDYRESYNCERFILESYNCSLAVE_H_

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/PThread.h>
#include <common/threading/SafeMutexLock.h>

class BuddyResyncerFileSyncSlave : public PThread
{
   friend class BuddyResyncer; // (to grant access to internal mutex)
   friend class BuddyResyncJob; // (to grant access to internal mutex)

   public:
      BuddyResyncerFileSyncSlave(uint16_t targetID, ChunkSyncCandidateStore* syncCandidates,
         uint8_t slaveID);
      virtual ~BuddyResyncerFileSyncSlave();

   private:
      AtomicSizeT onlyTerminateIfIdle; // atomic quasi-bool

      Mutex statusMutex; // protects isRunning
      Condition isRunningChangeCond;

      AtomicUInt64 numChunksSynced;
      AtomicUInt64 errorCount;

      bool isRunning; // true if an instance of this component is currently running

      uint16_t targetID;

      ChunkSyncCandidateStore* syncCandidates;

      virtual void run();
      void syncLoop();
      FhgfsOpsErr doResync(std::string& chunkPathStr, uint16_t localTargetID,
         uint16_t buddyTargetID);
      bool removeBuddyChunkUnlocked(Node& node, uint16_t buddyTargetID, std::string& pathStr);

   public:
      // getters & setters
      bool getIsRunning()
      {
         SafeMutexLock safeLock(&statusMutex);

         bool retVal = this->isRunning;

         safeLock.unlock();

         return retVal;
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

      uint64_t getNumChunksSynced()
      {
         return numChunksSynced.read();
      }

      uint64_t getErrorCount()
      {
         return errorCount.read();
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

typedef std::list<BuddyResyncerFileSyncSlave*> BuddyResyncerFileSyncSlaveList;
typedef BuddyResyncerFileSyncSlaveList::iterator BuddyResyncerFileSyncSlaveListIter;

typedef std::vector<BuddyResyncerFileSyncSlave*> BuddyResyncerFileSyncSlaveVec;
typedef BuddyResyncerFileSyncSlaveVec::iterator BuddyResyncerFileSyncSlaveVecIter;

#endif /* BUDDYRESYNCERFILESYNCSLAVE_H_ */
