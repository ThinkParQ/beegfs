#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/nodes/NodeStore.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/threading/PThread.h>
#include <common/Common.h>
#include <components/componenthelpers/CapacityInfo.h>
#include <components/componenthelpers/DemotionFlags.h>
#include <nodes/NodeStoreServersEx.h>
#include <nodes/TargetCapacityReport.h>

#include <mutex>

class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer();
      ~InternodeSyncer();


   private:
      LogContext log;

      Mutex forcePoolsUpdateMutex;
      bool forcePoolsUpdate; // true to force update of capacity pools
      Mutex forceCheckNetworkMutex;
      bool forceCheckNetwork;  // true to force check of network interfaces

      const unsigned targetOfflineTimeoutMS;

      RWLock targetCapacityReportMapLock;
      TargetCapacityReportMap targetCapacityReportMap;
      RWLock nodeCapacityReportMapLock;
      TargetCapacityReportMap nodeCapacityReportMap;

      virtual void run();
      void syncLoop();

      void updateMetaCapacityPools(bool updateForced);
      void updateStorageCapacityPools(bool updateForced);

      void updateNodeCapacityPools(NodeCapacityPools* pools, bool updateForced);
      void updateTargetCapacityPools(TargetCapacityPools* pools,
         TargetCapacityInfoList& targetCapacityInfos, bool updateForced);
      void updateMetaBuddyCapacityPools();
      void updateTargetBuddyCapacityPools();

      bool assignNodesToPools(NodeCapacityPools* pools, const CapacityInfoList& capacityInfos);
      bool assignNodeToPool(NodeCapacityPools* pools, NumNodeID nodeID, CapacityPoolType pool,
         const std::string& reason);

      void getTargetMappingAsInfoList(TargetMapper* targetMapper,
         TargetCapacityInfoList& outTargetCapacityInfos);
      std::map<StoragePoolId, TargetCapacityInfoList> getTargetMappingAsGroupedInfoLists();
      void getNodesAsInfoList(CapacityInfoList& outCapacityInfos);
      bool assignTargetsToPools(TargetCapacityPools* pools,
         const TargetCapacityInfoList& targetCapacityInfos);
      bool assignTargetToPool(TargetCapacityPools* pools, uint16_t targetID, NumNodeID nodeID,
         CapacityPoolType pool, const std::string& reason);

      template <typename P, typename L>
      void demoteIfNecessary(P* pools, L& capacityInfos, const DemotionFlags& demotionFlags);

      void logDemotionFlags(const DemotionFlags& demotionFlags, NodeType nodeType);

      void saveTargetMappings();
      void saveStates();

      void dropIdleConns();
      unsigned dropIdleConnsByStore(const NodeStoreServersEx* nodes);

      void clearStaleCapacityReports(const NodeType nodeType);

      bool checkNetwork();

   public:
      // getters & setters

      void setForcePoolsUpdate()
      {
         const std::lock_guard<Mutex> lock(forcePoolsUpdateMutex);

         this->forcePoolsUpdate = true;
      }

      void setForceCheckNetwork()
      {
         const std::lock_guard<Mutex> lock(forceCheckNetworkMutex);

         this->forceCheckNetwork = true;
      }

      // inliners

      bool getAndResetForcePoolsUpdate()
      {
         const std::lock_guard<Mutex> lock(forcePoolsUpdateMutex);

         bool retVal = this->forcePoolsUpdate;

         this->forcePoolsUpdate = false;

         return retVal;
      }

      bool getAndResetForceCheckNetwork()
      {
         const std::lock_guard<Mutex> lock(forceCheckNetworkMutex);

         bool retVal = this->forceCheckNetwork;

         this->forceCheckNetwork = false;

         return retVal;
      }

      /**
       * Take the elements of a StorageTargetInfoList and stores them in the local
       * target/nodeCapacityReportsMap as TargetCapacityReport objects.
       * @param nodeType the node type (storage / meta) for which the capacities are reported.
       */
      void storeCapacityReports(const StorageTargetInfoList& targetInfoList,
         const NodeType nodeType)
      {
         RWLock* reportsRWLock = (nodeType == NODETYPE_Meta)
            ? &nodeCapacityReportMapLock
            : &targetCapacityReportMapLock;

         TargetCapacityReportMap& capacityReportMap = (nodeType == NODETYPE_Meta)
            ? nodeCapacityReportMap
            : targetCapacityReportMap;

         Time timeNow; // Default-initialized to "now".

         RWLockGuard const lock(*reportsRWLock, SafeRWLock_WRITE);

         for (StorageTargetInfoListCIter targetInfoIter = targetInfoList.begin();
              targetInfoIter != targetInfoList.end(); ++targetInfoIter)
         {
            const StorageTargetInfo& targetInfo = *targetInfoIter;

            TargetCapacityReport capacityReport = {
               targetInfo.getDiskSpaceTotal(),
               targetInfo.getDiskSpaceFree(),
               targetInfo.getInodesTotal(),
               targetInfo.getInodesFree()
            };

            const uint16_t targetID = targetInfo.getTargetID();

            capacityReportMap[targetID] = capacityReport;
         }
      }
};

#endif /* INTERNODESYNCER_H_ */
