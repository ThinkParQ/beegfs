#ifndef BUDDYRESYNCJOB_H_
#define BUDDYRESYNCJOB_H_

#include <common/storage/mirroring/BuddyResyncJobStatistics.h>
#include <components/buddyresyncer/BuddyResyncerDirSyncSlave.h>
#include <components/buddyresyncer/BuddyResyncerFileSyncSlave.h>
#include <components/buddyresyncer/BuddyResyncerGatherSlave.h>

#define GATHERSLAVEQUEUE_MAXSIZE 5000

class BuddyResyncJob : public PThread
{
   friend class GenericDebugMsgEx;

   public:
      BuddyResyncJob(uint16_t targetID);
      virtual ~BuddyResyncJob();

      virtual void run();

      void abort();
      void getJobStats(StorageBuddyResyncJobStatistics& outStats);

   private:
      uint16_t targetID;
      Mutex statusMutex;
      BuddyResyncJobState status;

      int64_t startTime;
      int64_t endTime;

      ChunkSyncCandidateStore syncCandidates;
      BuddyResyncerGatherSlaveWorkQueue gatherSlavesWorkQueue;

      BuddyResyncerGatherSlaveVec gatherSlaveVec;
      BuddyResyncerFileSyncSlaveVec fileSyncSlaveVec;
      BuddyResyncerDirSyncSlaveVec dirSyncSlaveVec;

      // this thread walks over the top dir structures itself, so we need to track that
      AtomicUInt64 numDirsDiscovered;
      AtomicUInt64 numDirsMatched;

      AtomicInt16 shallAbort; // quasi-boolean
      AtomicInt16 targetWasOffline;

      bool checkTopLevelDir(std::string& path, int64_t lastBuddyCommTimeSecs);
      bool walkDirs(std::string chunksPath, std::string relPath, int level,
         int64_t lastBuddyCommTimeSecs);

      bool startGatherSlaves(const StorageTarget& target);
      bool startSyncSlaves();
      void joinGatherSlaves();
      void joinSyncSlaves();

   public:
      uint16_t getTargetID() const
      {
         return targetID;
      }

      BuddyResyncJobState getStatus()
      {
         std::lock_guard<Mutex> mutexLock(statusMutex);
         return status;
      }

      bool isRunning()
      {
         std::lock_guard<Mutex> mutexLock(statusMutex);
         return status == BuddyResyncJobState_RUNNING;
      }

      void setTargetOffline()
      {
         targetWasOffline.set(1);
      }

   private:
      void setStatus(BuddyResyncJobState status)
      {
         std::lock_guard<Mutex> mutexLock(statusMutex);
         this->status = status;
      }

      void informBuddy();
};

typedef std::map<uint16_t, BuddyResyncJob*> BuddyResyncJobMap; //mapping: targetID, job
typedef BuddyResyncJobMap::iterator BuddyResyncJobMapIter;


#endif /* BUDDYRESYNCJOB_H_ */
