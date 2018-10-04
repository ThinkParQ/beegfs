#ifndef BUDDYRESYNCERDIRSYNCSLAVE_H_
#define BUDDYRESYNCERDIRSYNCSLAVE_H_

#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/PThread.h>
#include <components/buddyresyncer/SyncCandidate.h>

class BuddyResyncerDirSyncSlave : public PThread
{
   friend class BuddyResyncer; // (to grant access to internal mutex)
   friend class BuddyResyncJob; // (to grant access to internal mutex)

   public:
      BuddyResyncerDirSyncSlave(uint16_t targetID, ChunkSyncCandidateStore* syncCandidates,
         uint8_t slaveID);
      virtual ~BuddyResyncerDirSyncSlave();

   private:
      Mutex statusMutex; // protects isRunning
      Condition isRunningChangeCond;

      AtomicSizeT onlyTerminateIfIdle;

      AtomicUInt64 numDirsSynced;
      AtomicUInt64 numAdditionalDirsMatched;
      AtomicUInt64 errorCount;

      bool isRunning; // true if an instance of this component is currently running

      uint16_t targetID;
      ChunkSyncCandidateStore* syncCandidates;

      virtual void run();
      void syncLoop();

      FhgfsOpsErr doSync(const std::string& dirPath, uint16_t localTargetID,
         uint16_t buddyTargetID);
      FhgfsOpsErr getBuddyDirContents(Node& node, const std::string& dirPath, uint16_t targetID,
         int64_t offset, StringList& outNames, IntList& outEntryTypes, int64_t& outNewOffset);
      FhgfsOpsErr findChunks(uint16_t targetID, const std::string& dirPath, StringList& inOutNames,
         IntList& inOutEntryTypes);
      FhgfsOpsErr removeBuddyChunkPaths(Node& node, uint16_t localTargetID, uint16_t buddyTargetID,
         StringList& paths);

   public:
      // getters & setters
      bool getIsRunning()
      {
         const std::lock_guard<Mutex> lock(statusMutex);

         return this->isRunning;
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

      uint64_t getNumDirsSynced()
      {
         return numDirsSynced.read();
      }

      uint64_t getNumAdditionalDirsMatched()
      {
         return numAdditionalDirsMatched.read();
      }

      uint64_t getErrorCount()
      {
         return errorCount.read();
      }

   private:
      // getters & setters

      void setIsRunning(bool isRunning)
      {
         const std::lock_guard<Mutex> lock(statusMutex);

         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();
      }

      bool getSelfTerminateNotIdle()
      {
         return ( (getSelfTerminate() && (!getOnlyTerminateIfIdle())) );
      }
};

typedef std::list<BuddyResyncerDirSyncSlave*> BuddyResyncerDirSyncSlaveList;
typedef BuddyResyncerDirSyncSlaveList::iterator BuddyResyncerDirSyncSlaveListIter;
typedef std::vector<BuddyResyncerDirSyncSlave*> BuddyResyncerDirSyncSlaveVec;
typedef BuddyResyncerDirSyncSlaveVec::iterator BuddyResyncerDirSyncSlaveVecIter;

#endif /* BUDDYRESYNCERDIRSYNCSLAVE_H_ */
