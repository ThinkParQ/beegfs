#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/storage/quota/QuotaData.h>
#include <common/threading/PThread.h>
#include <common/Common.h>
#include <storage/NodeOfflineWait.h>

#include <atomic>
#include <mutex>

class AbstractDatagramListener;

class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer(TargetConsistencyState initialConsistencyState);
      virtual ~InternodeSyncer() { }

      static bool registerNode(AbstractDatagramListener* dgramLis);

      static bool updateMetaStatesAndBuddyGroups(TargetConsistencyState& outConsistencyState,
         bool publish);
      static void syncClients(const std::vector<NodeHandle>& clientsList, bool allowRemoteComm);
      static bool downloadAndSyncNodes();
      static bool downloadAndSyncTargetMappings();
      static bool downloadAndSyncStoragePools();
      static bool downloadAndSyncTargetStatesAndBuddyGroups();

      static void downloadAndSyncClients(bool requeue);

      static bool updateMetaCapacityPools();
      static bool updateMetaBuddyCapacityPools();

      static bool downloadAllExceededQuotaLists(const StoragePoolPtrVec& storagePools);
      static bool downloadExceededQuotaList(StoragePoolId storagePoolId, QuotaDataType idType,
         QuotaLimitType exType, UIntList* outIDList, FhgfsOpsErr& error);

      static void printSyncResults(NodeType nodeType, NumNodeIDList* addedNodes,
         NumNodeIDList* removedNodes);

   private:
      LogContext log;

#if ATOMIC_BOOL_LOCK_FREE != 2
# warn atomic<bool> is not always lock-free
#endif
      std::atomic<bool> forcePoolsUpdate; // true to force update of capacity pools
      std::atomic<bool> forceTargetStatesUpdate; // true to force update of node state
      std::atomic<bool> forcePublishCapacities; // true to force publishing free capacity
      std::atomic<bool> forceStoragePoolsUpdate; // true to force update of storage pools
      std::atomic<bool> forceCheckNetwork; // true to force checking of network changes

      // Keeps track of the timeout during which the node may not send state reports because it is
      // waiting to be offlined by the mgmtd.
      NodeOfflineWait offlineWait;

      Mutex nodeConsistencyStateMutex;
      TargetConsistencyState nodeConsistencyState; // Node's own consistency state.
      // Note: This is initialized when updateMetaStates... is called from App::downloadMgmtInfo.
      AtomicUInt32 buddyResyncInProgress;

      virtual void run();
      void syncLoop();

      static bool updateStorageCapacityPools();
      static bool updateTargetBuddyCapacityPools();
      static std::pair<bool, GetNodeCapacityPoolsRespMsg::PoolsMap> downloadCapacityPools(
         CapacityPoolQueryType poolType);
      void publishNodeCapacity();

      void forceMgmtdPoolsRefresh();

      // returns true if the local interfaces have changed
      bool checkNetwork();
      void dropIdleConns();
      unsigned dropIdleConnsByStore(NodeStoreServers* nodes);

      void getStatInfo(int64_t* outSizeTotal, int64_t* outSizeFree, int64_t* outInodesTotal,
         int64_t* outInodesFree);

      static TargetConsistencyState decideResync(const CombinedTargetState newState);
      static bool publishNodeStateChange(const TargetConsistencyState oldState,
         const TargetConsistencyState newState);

      static bool downloadAllExceededQuotaLists(const StoragePoolPtr storagePool);

   public:
      // inliners
      void setForcePoolsUpdate()
      {
         forcePoolsUpdate = true;
      }

      void setForceTargetStatesUpdate()
      {
         forceTargetStatesUpdate = true;
      }

      void setForcePublishCapacities()
      {
         forcePublishCapacities = true;
      }

      void setForceStoragePoolsUpdate()
      {
         forceStoragePoolsUpdate = true;
      }

      void setForceCheckNetwork()
      {
         forceCheckNetwork = true;
      }

      TargetConsistencyState getNodeConsistencyState()
      {
         std::lock_guard<Mutex> lock(nodeConsistencyStateMutex);
         return nodeConsistencyState;
      }

      void setNodeConsistencyState(TargetConsistencyState newState)
      {
         std::lock_guard<Mutex> lock(nodeConsistencyStateMutex);
         nodeConsistencyState = newState;
      }

      void setResyncInProgress(bool resyncInProgress)
      {
         this->buddyResyncInProgress.set(resyncInProgress);
      }

      bool getResyncInProgress()
      {
         return this->buddyResyncInProgress.read();
      }
};


#endif /* INTERNODESYNCER_H_ */
