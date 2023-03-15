#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/AbstractDatagramListener.h>
#include <common/components/ComponentInitException.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/threading/PThread.h>
#include <common/Common.h>

class StorageTarget;

class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer();
      virtual ~InternodeSyncer();

      static bool downloadAndSyncTargetStates(UInt16List& outTargetIDs,
         UInt8List& outReachabilityStates, UInt8List& outConsistencyStates);
      static bool downloadAndSyncNodes();
      static bool downloadAndSyncTargetMappings();
      static bool downloadAndSyncMirrorBuddyGroups();
      static bool downloadAndSyncStoragePools();

      static bool downloadAllExceededQuotaLists(
            const std::map<uint16_t, std::unique_ptr<StorageTarget>>& targets);
      static bool downloadExceededQuotaList(uint16_t targetId, QuotaDataType idType,
         QuotaLimitType exType, UIntList* outIDList, FhgfsOpsErr& error);

      static void syncClientSessions(const std::vector<NodeHandle>& clientsList);

      void publishTargetState(uint16_t targetID, TargetConsistencyState targetState);

      bool publishLocalTargetStateChanges(const TargetStateMap& oldStates,
         const TargetStateMap& changes);

      static bool registerNode(AbstractDatagramListener* dgramLis);
      static bool registerTargetMappings();
      static void requestBuddyTargetStates();

   private:
      LogContext log;

      Mutex forceTargetStatesUpdateMutex;
      Mutex forcePublishCapacitiesMutex;
      Mutex forceStoragePoolsUpdateMutex;
      Mutex forceCheckNetworkMutex;
      bool forceTargetStatesUpdate; // true to force update of target states
      bool forcePublishCapacities; // true to force publishing target capacities
      bool forceStoragePoolsUpdate; // true to force update of storage pools
      bool forceCheckNetwork; // true to force update of network interfaces

      virtual void run();
      void syncLoop();

      // returns true if the local interfaces have changed
      bool checkNetwork();
      void dropIdleConns();
      unsigned dropIdleConnsByStore(NodeStoreServers* nodes);

      void updateTargetStatesAndBuddyGroups();
      void publishTargetCapacities();

      void forceMgmtdPoolsRefresh();

      static void printSyncNodesResults(NodeType nodeType, NumNodeIDList* addedNodes,
         NumNodeIDList* removedNodes);

      bool publishTargetStateChanges(UInt16List& targetIDs, UInt8List& oldStates,
         UInt8List& newStates);

      static bool downloadAllExceededQuotaLists(uint16_t targetId);
   public:
      // inliners
      void setForceTargetStatesUpdate()
      {
         std::lock_guard<Mutex> safeLock(forceTargetStatesUpdateMutex);
         this->forceTargetStatesUpdate = true;
      }

      void setForcePublishCapacities()
      {
         std::lock_guard<Mutex> safeLock(forcePublishCapacitiesMutex);
         this->forcePublishCapacities = true;
      }

      void setForceStoragePoolsUpdate()
      {
         std::lock_guard<Mutex> lock(forceStoragePoolsUpdateMutex);
         forceStoragePoolsUpdate = true;
      }

      void setForceCheckNetwork()
      {
         std::lock_guard<Mutex> lock(forceCheckNetworkMutex);
         forceCheckNetwork = true;
      }

   private:
      // inliners
      bool getAndResetForceTargetStatesUpdate()
      {
         std::lock_guard<Mutex> safeLock(forceTargetStatesUpdateMutex);

         bool retVal = this->forceTargetStatesUpdate;

         this->forceTargetStatesUpdate = false;

         return retVal;
      }

      bool getAndResetForcePublishCapacities()
      {
         std::lock_guard<Mutex> safeLock(forcePublishCapacitiesMutex);

         bool retVal = this->forcePublishCapacities;

         this->forcePublishCapacities = false;

         return retVal;
      }

      bool getAndResetForceStoragePoolsUpdate()
      {
         std::lock_guard<Mutex> lock(forceStoragePoolsUpdateMutex);

         bool retVal = forceStoragePoolsUpdate;
         forceStoragePoolsUpdate = false;

         return retVal;
      }

      bool getAndResetForceCheckNetwork()
      {
         std::lock_guard<Mutex> lock(forceCheckNetworkMutex);

         bool retVal = forceCheckNetwork;
         forceCheckNetwork = false;

         return retVal;
      }
};


#endif /* INTERNODESYNCER_H_ */
