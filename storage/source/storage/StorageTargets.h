#ifndef STORAGETARGETS_H_
#define STORAGETARGETS_H_


#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/threading/RWLockGuard.h>
#include <common/toolkit/PreallocatedFile.h>
#include <common/components/TimerQueue.h>
#include <app/config/Config.h>
#include <storage/QuotaBlockDevice.h>

#include <boost/optional.hpp>
#include <atomic>
#include <chrono>

class NodeStoreServers;
class MirrorBuddyGroupMapper;

class StorageTarget
{
   public:
      StorageTarget(Path path, uint16_t targetID, TimerQueue& timerQueue,
            NodeStoreServers& mgmtNodes, MirrorBuddyGroupMapper& buddyGroupMapper);

      ~StorageTarget()
      {
         if (setBuddyNeedsResyncEntry)
            setBuddyNeedsResyncEntry->cancel();
      }

      static void prepareTargetDir(const Path& path);

      static bool isTargetDir(const Path& path);

      const Path& getPath() const { return path; }
      uint16_t getID() const { return id; }
      const FDHandle& getChunkFD() const { return chunkFD; }
      const FDHandle& getMirrorFD() const { return mirrorFD; }
      const QuotaBlockDevice& getQuotaBlockDevice() const { return quotaBlockDevice; }

      TargetConsistencyState getConsistencyState() const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);
         return consistencyState;
      }

      void setConsistencyState(TargetConsistencyState tcs)
      {
         RWLockGuard const lock(rwlock, SafeRWLock_WRITE);
         consistencyState = tcs;
      }

      bool getBuddyResyncInProgress() const { return buddyResyncInProgress; }
      void setBuddyResyncInProgress(bool b) { buddyResyncInProgress = b; }

      bool getCleanShutdown() const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);
         return cleanShutdown;
      }

      void setCleanShutdown(bool b)
      {
         RWLockGuard const lock(rwlock, SafeRWLock_WRITE);
         cleanShutdown = b;
      }

      void setOfflineTimeout(std::chrono::milliseconds timeout)
      {
         RWLockGuard const lock(rwlock, SafeRWLock_WRITE);
         offlineTimeoutEnd = std::chrono::steady_clock::now() + timeout;
      }

      boost::optional<std::chrono::milliseconds> getOfflineTimeout() const
      {
         {
            RWLockGuard const lock(rwlock, SafeRWLock_READ);
            if (!offlineTimeoutEnd)
              return boost::none;

            const auto remaining = *offlineTimeoutEnd - std::chrono::steady_clock::now();
            if (remaining.count() > 0)
               return std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
         }

         RWLockGuard const lock(rwlock, SafeRWLock_WRITE);
         offlineTimeoutEnd = boost::none;
         return boost::none;
      }

      bool getBuddyNeedsResync() const
      {
         return (buddyNeedsResyncFile.read().get_value_or(0) & BUDDY_RESYNC_REQUIRED_FLAG) != 0;
      }

      void setBuddyNeedsResync(bool needsResync);

      std::pair<bool, std::chrono::system_clock::time_point> getLastBuddyComm() const
      {
         const auto state = lastBuddyCommFile.read().get_value_or({});
         if (state.overrideSecs == 0)
            return {false, std::chrono::system_clock::from_time_t(state.lastCommSecs)};
         else
            return {true, std::chrono::system_clock::from_time_t(state.overrideSecs)};
      }

      void setLastBuddyComm(std::chrono::system_clock::time_point time, bool isOverride)
      {
         RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

         auto state = lastBuddyCommFile.read().get_value_or({});
         if (isOverride)
            state.overrideSecs = std::chrono::system_clock::to_time_t(time);
         else
            state.lastCommSecs = std::chrono::system_clock::to_time_t(time);

         lastBuddyCommFile.write(state);
      }

      void setState(TargetConsistencyState newState)
      {
         const auto stateChanged = [&] {
            const RWLockGuard lock(rwlock, SafeRWLock_WRITE);

            const auto oldState = consistencyState;
            consistencyState = newState;
            LOG_DBG(STATES, DEBUG, "Changing target consistency state.", id,
                  ("oldState", TargetStateStore::stateToStr(oldState)),
                  ("newState", TargetStateStore::stateToStr(newState)),
                  ("Called from", Backtrace<3>()));

            return oldState != newState;
         }();

         if (stateChanged)
            handleTargetStateChange();
      }

   private:
      struct LastBuddyComm
      {
         int64_t lastCommSecs;
         int64_t overrideSecs;

         template<typename This, typename Ctx>
         static void serialize(This obj, Ctx& ctx)
         {
            ctx
               % obj->lastCommSecs
               % obj->overrideSecs;
         }
      };

      enum {
         BUDDY_RESYNC_UNACKED_FLAG = 1,
         BUDDY_RESYNC_REQUIRED_FLAG = 2,

         BUDDY_RESYNC_NOT_REQUIRED = 0,
         BUDDY_RESYNC_NOT_REQUIRED_UNACKED = BUDDY_RESYNC_UNACKED_FLAG,
         BUDDY_RESYNC_REQUIRED = BUDDY_RESYNC_REQUIRED_FLAG,
         BUDDY_RESYNC_REQUIRED_UNACKED = BUDDY_RESYNC_REQUIRED_FLAG | BUDDY_RESYNC_UNACKED_FLAG,
      };

      Path path;
      uint16_t id;
      FDHandle chunkFD;
      FDHandle mirrorFD;
      PreallocatedFile<uint8_t> buddyNeedsResyncFile;
      PreallocatedFile<LastBuddyComm> lastBuddyCommFile;
      QuotaBlockDevice quotaBlockDevice; // quota related information about the block device
      TimerQueue& timerQueue;
      NodeStoreServers& mgmtNodes;
      MirrorBuddyGroupMapper& buddyGroupMapper;

      std::atomic<bool> buddyResyncInProgress;

      mutable RWLock rwlock;
      TargetConsistencyState consistencyState;
      bool cleanShutdown; // Was the previous session cleanly shut down?
      mutable boost::optional<std::chrono::steady_clock::time_point> offlineTimeoutEnd;
      boost::optional<TimerQueue::EntryHandle> setBuddyNeedsResyncEntry;

      bool setBuddyNeedsResyncComm();

      void handleTargetStateChange();

      void retrySetBuddyNeedsResyncComm()
      {
         const RWLockGuard lock(rwlock, SafeRWLock_WRITE);
         setBuddyNeedsResyncComm();
      }
};

typedef std::map<uint16_t, std::unique_ptr<StorageTarget>> StorageTargetMap;
typedef StorageTargetMap::iterator StorageTargetMapIter;
typedef StorageTargetMap::const_iterator StorageTargetMapCIter;

typedef std::map<uint16_t, TargetConsistencyState> ConsistencyStateMap;
typedef ConsistencyStateMap::iterator ConsistencyStateMapIter;
typedef ConsistencyStateMap::const_iterator ConsistencyStateMapCIter;
typedef ConsistencyStateMap::value_type ConsistencyStateMapVal;

typedef std::map<uint16_t, unsigned> TargetNeedsResyncCountMap;

class StorageTargets
{
   public:
      StorageTargets(std::map<uint16_t, std::unique_ptr<StorageTarget>> targets):
         storageTargetMap(std::move(targets))
      {
      }

      void decideResync(const TargetStateMap& statesFromMgmtd,
         TargetStateMap& outLocalStateChanges);
      void checkBuddyNeedsResync();

      void generateTargetInfoList(StorageTargetInfoList& outTargetInfoList);

      static void fillTargetStateMap(const UInt16List& targetIDs,
         const UInt8List& reachabilityStates, const UInt8List& consistencyStates,
         TargetStateMap& outStateMap);

   private:
      const StorageTargetMap storageTargetMap; // keys: targetIDs; values: StorageTargetData

      void handleTargetStateChange(uint16_t targetID, TargetConsistencyState newState);

      static void getStatInfo(std::string& targetPathStr, int64_t* outSizeTotal,
         int64_t* outSizeFree, int64_t* outInodesTotal, int64_t* outInodesFree);

   public:
      const StorageTarget* getTarget(uint16_t id) const
      {
         return const_cast<StorageTargets*>(this)->getTarget(id);
      }

      StorageTarget* getTarget(uint16_t id)
      {
         const auto it = storageTargetMap.find(id);
         return it != storageTargetMap.end()
            ? it->second.get()
            : nullptr;
      }

      const std::map<uint16_t, std::unique_ptr<StorageTarget>>& getTargets() const
      {
         return storageTargetMap;
      }
};


#endif /* STORAGETARGETS_H_ */
