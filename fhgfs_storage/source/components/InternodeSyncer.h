#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <nodes/NodeStoreServersEx.h>
#include <common/app/log/LogContext.h>
#include <common/components/AbstractDatagramListener.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>
#include <common/Common.h>


class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer() throw(ComponentInitException);
      virtual ~InternodeSyncer();

      static bool downloadAndSyncTargetStates(UInt16List& outTargetIDs,
         UInt8List& outReachabilityStates, UInt8List& outConsistencyStates);
      static bool downloadAndSyncNodes();
      static bool downloadAndSyncTargetMappings();
      static bool downloadAndSyncMirrorBuddyGroups();

      static bool downloadAllExceededQuotaLists();
      static bool downloadExceededQuotaList(QuotaDataType idType, QuotaLimitType exType,
         UIntList* outIDList, FhgfsOpsErr& error);

      static void syncClientSessions(const std::vector<NodeHandle>& clientsList);

      void publishTargetState(uint16_t targetID, TargetConsistencyState targetState);

      bool publishLocalTargetStateChanges(UInt16List& targetIDs, UInt8List& oldStates,
         UInt8List& newStates);

      static bool registerNode(AbstractDatagramListener* dgramLis);
      static bool registerTargetMappings();
      static void requestBuddyTargetStates();

   private:
      LogContext log;

      Mutex forceTargetStatesUpdateMutex;
      Mutex forcePublishCapacitiesMutex;
      bool forceTargetStatesUpdate; // true to force update of target states
      bool forcePublishCapacities; // true to force publishing target capacities

      virtual void run();
      void syncLoop();

      void dropIdleConns();
      unsigned dropIdleConnsByStore(NodeStoreServersEx* nodes);

      void updateTargetStatesAndBuddyGroups();
      void publishTargetCapacities();

      void forceMgmtdPoolsRefresh();

      static void printSyncNodesResults(NodeType nodeType, NumNodeIDList* addedNodes,
         NumNodeIDList* removedNodes);

      bool publishTargetStateChanges(UInt16List& targetIDs, UInt8List& oldStates,
         UInt8List& newStates);

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
};


#endif /* INTERNODESYNCER_H_ */
