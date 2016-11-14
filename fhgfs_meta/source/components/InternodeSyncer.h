#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>
#include <common/storage/quota/QuotaData.h>
#include <common/threading/PThread.h>
#include <common/Common.h>
#include <nodes/NodeStoreServersEx.h>
#include <storage/NodeOfflineWait.h>

#include <mutex>

class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer(TargetConsistencyState initialConsistencyState) throw(ComponentInitException);
      virtual ~InternodeSyncer() { }

      static bool registerNode(AbstractDatagramListener* dgramLis);

      static bool updateMetaStatesAndBuddyGroups(TargetConsistencyState& outConsistencyState,
         bool publish);
      static void syncClients(const std::vector<NodeHandle>& clientsList, bool allowRemoteComm);
      static bool downloadAndSyncNodes();
      static bool downloadAndSyncTargetMappings();
      static bool downloadAndSyncTargetStatesAndBuddyGroups();

      static void downloadAndSyncClients(bool requeue);

      static bool downloadAllExceededQuotaLists();
      static bool downloadExceededQuotaList(QuotaDataType idType, QuotaLimitType exType,
         UIntList* outIDList, FhgfsOpsErr& error);

      static void printSyncResults(NodeType nodeType, NumNodeIDList* addedNodes,
         NumNodeIDList* removedNodes);

   private:
      LogContext log;

      Mutex forcePoolsUpdateMutex;
      Mutex forceTargetStatesUpdateMutex;
      Mutex forcePublishCapacitiesMutex;
      bool forcePoolsUpdate; // true to force update of capacity pools
      bool forceTargetStatesUpdate; // true to force update of node state
      bool forcePublishCapacities; // true to force publishing free capacity

      // Keeps track of the timeout during which the node may not send state reports because it is
      // waiting to be offlined by the mgmtd.
      NodeOfflineWait offlineWait;

      TargetConsistencyState nodeConsistencyState; // Node's own consistency state.
      // Note: This is initialized when updateMetaStates... is called from App::downloadMgmtInfo.
      AtomicUInt32 buddyResyncInProgress;

      virtual void run();
      void syncLoop();

      void updateMetaCapacityPools();
      void updateMetaBuddyCapacityPools();
      void updateStorageCapacityPools();
      void updateTargetBuddyCapacityPools();
      bool downloadCapacityPools(CapacityPoolQueryType poolType, UInt16List* outListNormal,
         UInt16List* outListLow, UInt16List* outListEmergency);
      void publishNodeCapacity();

      void forceMgmtdPoolsRefresh();

      void dropIdleConns();
      unsigned dropIdleConnsByStore(NodeStoreServersEx* nodes);

      void getStatInfo(int64_t* outSizeTotal, int64_t* outSizeFree, int64_t* outInodesTotal,
         int64_t* outInodesFree);

      static TargetConsistencyState decideResync(const CombinedTargetState newState);
      static bool publishNodeStateChange(const TargetConsistencyState oldState,
         const TargetConsistencyState newState);

   public:
      // inliners
      void setForcePoolsUpdate()
      {
         std::lock_guard<Mutex> lock(forcePoolsUpdateMutex);
         forcePoolsUpdate = true;
      }

      void setForceTargetStatesUpdate()
      {
         std::lock_guard<Mutex> lock(forceTargetStatesUpdateMutex);
         forceTargetStatesUpdate = true;
      }

      void setForcePublishCapacities()
      {
         std::lock_guard<Mutex> lock(forcePublishCapacitiesMutex);
         forcePublishCapacities = true;
      }

      TargetConsistencyState getNodeConsistencyState()
      {
         return this->nodeConsistencyState;
      }

      void setNodeConsistencyState(TargetConsistencyState newState)
      {
         this->nodeConsistencyState = newState;
      }

      void setResyncInProgress(bool resyncInProgress)
      {
         this->buddyResyncInProgress.set(resyncInProgress);
      }

      bool getResyncInProgress()
      {
         return this->buddyResyncInProgress.read();
      }

   private:
      // inliners
      bool getAndResetForcePoolsUpdate()
      {
         std::lock_guard<Mutex> lock(forcePoolsUpdateMutex);

         bool retVal = forcePoolsUpdate;
         forcePoolsUpdate = false;

         return retVal;
      }

      bool getAndResetForceTargetStatesUpdate()
      {
         std::lock_guard<Mutex> lock(forceTargetStatesUpdateMutex);

         bool retVal = forceTargetStatesUpdate;
         forceTargetStatesUpdate = false;

         return retVal;
      }

      bool getAndResetForcePublishCapacities()
      {
         std::lock_guard<Mutex> lock(forcePublishCapacitiesMutex);

         bool retVal = forcePublishCapacities;
         forcePublishCapacities = false;

         return retVal;
      }
};


#endif /* INTERNODESYNCER_H_ */
