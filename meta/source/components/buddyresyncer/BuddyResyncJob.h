#ifndef BUDDYRESYNCJOB_H_
#define BUDDYRESYNCJOB_H_

#include <components/buddyresyncer/BuddyResyncerGatherSlave.h>
#include <components/buddyresyncer/SessionStoreResyncer.h>
#include <common/storage/mirroring/BuddyResyncJobStatistics.h>
#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/threading/PThread.h>
#include <common/nodes/NumNodeID.h>
#include <common/nodes/TargetStateInfo.h>

#include <atomic>
#include <mutex>

class BuddyResyncerBulkSyncSlave;
class BuddyResyncerModSyncSlave;

class BuddyResyncJob : public PThread
{
   public:
      BuddyResyncJob();
      ~BuddyResyncJob();

      virtual void run();

      void abort(bool wait_for_completion);
      MetaBuddyResyncJobStatistics getJobStats();
      std::atomic<unsigned int> threadCount{ 0 };

   private:
      BuddyResyncJobState state;
      Mutex stateMutex;

      int64_t startTime;
      int64_t endTime;

      NumNodeID buddyNodeID;

      MetaSyncCandidateStore syncCandidates;

      std::unique_ptr<BuddyResyncerGatherSlave> gatherSlave;
      std::vector<std::unique_ptr<BuddyResyncerBulkSyncSlave>> bulkSyncSlaves;
      std::unique_ptr<BuddyResyncerModSyncSlave> modSyncSlave;
      std::unique_ptr<SessionStoreResyncer> sessionStoreResyncer;

      bool startGatherSlaves();
      bool startSyncSlaves();
      void joinGatherSlaves();

   public:
      BuddyResyncJobState getState()
      {
         std::lock_guard<Mutex> lock(stateMutex);
         return state;
      }

      bool isRunning()
      {
         std::lock_guard<Mutex> lock(stateMutex);
         return state == BuddyResyncJobState_RUNNING;
      }

      void enqueue(MetaSyncCandidateFile syncCandidate, PThread* caller)
      {
         syncCandidates.add(std::move(syncCandidate), caller);
      }

      void registerOps()
      {
	 this->threadCount += 1;
      }

      void unregisterOps()
      {
	 this->threadCount -= 1;
      }

   private:
      void setState(const BuddyResyncJobState state)
      {
         LOG_DEBUG(__func__, Log_DEBUG, "Setting state: "
            + StringTk::uintToStr(static_cast<int>(state) ) );
         std::lock_guard<Mutex> lock(stateMutex);
         this->state = state;
      }

      TargetConsistencyState newBuddyState();
      void informBuddy(const TargetConsistencyState newTargetState);
      void informMgmtd(const TargetConsistencyState newTargetState);

      void stopAllWorkersOn(Barrier& barrier);
};

#endif /* BUDDYRESYNCJOB_H_ */
