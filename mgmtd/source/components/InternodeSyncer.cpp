#include "InternodeSyncer.h"

#include <common/net/message/storage/StatStoragePathMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/MinMaxStore.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/ZipIterator.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeStore.h>
#include <common/nodes/TargetCapacityPools.h>
#include <app/App.h>
#include <app/config/Config.h>
#include <nodes/StoragePoolStoreEx.h>
#include <storage/StoragePoolEx.h>
#include <program/Program.h>

#include <boost/lexical_cast.hpp>

InternodeSyncer::InternodeSyncer():
   PThread("XNodeSync"),
   log("XNodeSync"),
   forcePoolsUpdate(false),
   targetOfflineTimeoutMS(Program::getApp()->getConfig()->getSysTargetOfflineTimeoutSecs() * 1000)
{
}

InternodeSyncer::~InternodeSyncer()
{
}


void InternodeSyncer::run()
{
   try
   {
      registerSignalHandler();

      syncLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

   saveTargetMappings();
   saveStates();
}

void InternodeSyncer::syncLoop()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   MgmtdTargetStateStore* mgmtdTargetStateStore = app->getTargetStateStore();
   MgmtdTargetStateStore* mgmtdMetaStateStore = app->getMetaStateStore();

   bool askForCapacities = true;

   const int sleepIntervalMS = 5*1000; // 5sec

   // If (undocumented) sysUpdateTargetStatesSecs is set in config, use that value, otherwise
   // default to 1/6 sysTargetOfflineTimeoutSecs.
   const unsigned updateStatesMS =
      (cfg->getSysUpdateTargetStatesSecs() != 0)
      ? cfg->getSysUpdateTargetStatesSecs() * 1000
      : cfg->getSysTargetOfflineTimeoutSecs() * 166;

   // Set POffline timout to half the Offline timeout.
   const unsigned targetPOfflineTimeoutMS = cfg->getSysTargetOfflineTimeoutSecs() * 500;

   // Choosing those timeouts ensures that there are at least 3 calls to autoOfflineTarget
   // before the POffline timeout is passed, and three more before the Offline timeout.
   // The updateStates interval is also used by the other nodes to ask the Mgmtd for the states,
   // which should happen at least twice between POffline and Offline timeouts - 1/3 of the
   // Offline - POffline interval ensures that, even if the remote InternodeSyncer is delayed for
   // some reason.

   // Is very short on the first loop, so that the targets can be assigned to pools quickly. When
   // the first capacity pools update has happened, this delay will be increased.
   unsigned updateCapacityPoolsMS = 15 * 1000;

   const unsigned saveTargetMappingsMS = 15*60*1000; // 15min
   const unsigned idleDisconnectIntervalMS = 70*60*1000; /* 70 minutes (must be less than half the
      streamlis idle disconnect interval to avoid cases where streamlis disconnects first) */

   const unsigned checkNetworkIntervalMS = 60*1000; // 1 minute

   Time lastStatesUpdateT;
   Time lastCapacityPoolsUpdateT;
   Time lastTargetMappingsSaveT;
   Time lastIdleDisconnectT;
   Time lastCheckNetworkT;
   bool doUpdatePeers = false;

   while(!waitForSelfTerminateOrder(sleepIntervalMS))
   {
      // On the first loop, send out a request for the metadata and storage servers to publish
      // their free capacity information. This way we can assign them to capacity pools with a much
      // shorter delay upon mgmtd startup.
      // Note: This is done on the first loop (and not immediately at startup) so the network
      //       listener is initialized and nodes don't get a 'soft disconnect' on the first try.
      if (askForCapacities)
      {
         app->getHeartbeatMgr()->notifyAsyncPublishCapacities();
         askForCapacities = false;
      }


      bool checkNetworkForced = getAndResetForceCheckNetwork();

      if( checkNetworkForced ||
         (lastCheckNetworkT.elapsedMS() > checkNetworkIntervalMS))
      {
         if (checkNetwork())
            doUpdatePeers = true;
         lastCheckNetworkT.setToNow();
      }

      if (doUpdatePeers)
         doUpdatePeers = !app->getHeartbeatMgr()->notifyNodes();

      bool poolsUpdateForced = getAndResetForcePoolsUpdate();

      if( poolsUpdateForced ||
         (lastStatesUpdateT.elapsedMS() > updateStatesMS) )
      {
         bool statesModified = false;

         statesModified |= mgmtdTargetStateStore->autoOfflineTargets(targetPOfflineTimeoutMS,
            targetOfflineTimeoutMS, app->getStorageBuddyGroupMapper() );

         statesModified |= mgmtdMetaStateStore->autoOfflineTargets(
            targetPOfflineTimeoutMS, targetOfflineTimeoutMS, app->getMetaBuddyGroupMapper() );

         statesModified |= mgmtdTargetStateStore->resolvePrimaryResync();

         statesModified |= mgmtdMetaStateStore->resolvePrimaryResync();

         // if target states have changed, inform other nodes about it
         if (statesModified)
            app->getHeartbeatMgr()->notifyAsyncRefreshTargetStates();

         saveStates();

         lastStatesUpdateT.setToNow();
      }

      if( poolsUpdateForced ||
         (lastCapacityPoolsUpdateT.elapsedMS() > updateCapacityPoolsMS) )
      {
         updateMetaCapacityPools(poolsUpdateForced);
         updateStorageCapacityPools(poolsUpdateForced);

         lastCapacityPoolsUpdateT.setToNow();

         // Increase timeout after first capacity update.
         updateCapacityPoolsMS = 6 * updateStatesMS;
      }

      if(lastTargetMappingsSaveT.elapsedMS() > saveTargetMappingsMS)
      {
         saveTargetMappings();
         lastTargetMappingsSaveT.setToNow();
      }

      if(lastIdleDisconnectT.elapsedMS() > idleDisconnectIntervalMS)
      {
         dropIdleConns();
         lastIdleDisconnectT.setToNow();
      }
   }
}


/**
 * Inspect the available and allowed network interfaces for any changes.
 */
bool InternodeSyncer::checkNetwork()
{
   App* app = Program::getApp();
   NicAddressList newLocalNicList;
   bool res = false;

   app->findAllowedInterfaces(newLocalNicList);
   if (!std::equal(newLocalNicList.begin(), newLocalNicList.end(), app->getLocalNicList().begin()))
   {
      log.log(Log_NOTICE, "checkNetwork: local interfaces have changed");
      app->updateLocalNicList(newLocalNicList);
      res = true;
   }

   return res;
}

void InternodeSyncer::updateMetaCapacityPools(bool updateForced)
{
   App* app = Program::getApp();
   NodeCapacityPools* pools = app->getMetaCapacityPools();

   updateNodeCapacityPools(pools, updateForced);
}

void InternodeSyncer::updateStorageCapacityPools(bool updateForced)
{
   const App* app = Program::getApp();

   auto targetCapacityInfos = getTargetMappingAsGroupedInfoLists();
   clearStaleCapacityReports(NODETYPE_Storage);

   for (auto iter = targetCapacityInfos.begin(); iter != targetCapacityInfos.end(); iter++)
   {
      const StoragePoolId storagePoolId = iter->first;
      const StoragePoolExPtr storagePool = app->getStoragePoolStore()->getPool(storagePoolId);

      if (!storagePool)
         continue;

      updateTargetCapacityPools(storagePool->getTargetCapacityPools(), iter->second, updateForced);
   }
}

/**
 * Update capacity pool assignments for metadata servers.
 */
void InternodeSyncer::updateNodeCapacityPools(NodeCapacityPools* pools, bool updateForced)
{
   LogContext("Update Node CapPools").log(LogTopic_STATES, Log_DEBUG,
         "Starting node capacity pools update.");
   bool poolsModified = false; // true if pool lists changed and we need to inform others about it

   // minima and maxima of the pools' free capacity - needed for adaptive pooling
   MinMaxStore<int64_t> normalPoolFreeSpaceMinMax;
   MinMaxStore<int64_t> lowPoolFreeSpaceMinMax;
   MinMaxStore<int64_t> normalPoolInodesMinMax;
   MinMaxStore<int64_t> lowPoolInodesMinMax;

   CapacityInfoList capacityInfos;
   getNodesAsInfoList(capacityInfos);

   clearStaleCapacityReports(NODETYPE_Meta);

   {
      RWLockGuard const lock(nodeCapacityReportMapLock, SafeRWLock_READ);

      for (CapacityInfoListIter capacityInfoIter = capacityInfos.begin();
           capacityInfoIter != capacityInfos.end(); /* iterator increment in loop */ )
      {
         CapacityInfo& capacityInfo = *capacityInfoIter;

         // Find node ID in map.
         TargetCapacityReportMapIter capacityReportIter =
            nodeCapacityReportMap.find(capacityInfo.getNodeID().val() );
         if (capacityReportIter == nodeCapacityReportMap.end() )
         {  // No known capacity report for the node. Either it's not (yet) reported, or has not
            // sent a report since the last capacity pools update. Carefully move the node to the
            // emergency pool to at least avoid new allocations on this node when other nodes are
            // still available.
            bool updateRes = assignNodeToPool(pools, capacityInfo.getNodeID(),
               CapacityPool_EMERGENCY, "No capacity report received.");
            poolsModified |= updateRes;

            // Remove node from list (it is now assigned to a pool).
            capacityInfoIter = capacityInfos.erase(capacityInfoIter);
            continue;
         }

         TargetCapacityReport& capacityReport = capacityReportIter->second;
         int64_t freeSpace = capacityReport.diskSpaceFree;
         int64_t freeInodes = capacityReport.inodesFree;

         CapacityPoolType poolTypeFreeSpace =
            pools->getPoolLimitsSpace().getPoolTypeFromFreeCapacity(freeSpace);
         CapacityPoolType poolTypeInodes =
            pools->getPoolLimitsInodes().getPoolTypeFromFreeCapacity(freeInodes);

         capacityInfo.setFreeSpace(freeSpace);
         capacityInfo.updateTargetPoolType(poolTypeFreeSpace);
         capacityInfo.setFreeInodes(freeInodes);
         capacityInfo.updateTargetPoolType(poolTypeInodes);

         // store minimum / maximum free space for each pool

         if(poolTypeFreeSpace == CapacityPool_NORMAL)
            normalPoolFreeSpaceMinMax.enter(freeSpace);
         else
         if(poolTypeFreeSpace == CapacityPool_LOW)
            lowPoolFreeSpaceMinMax.enter(freeSpace);

         if(poolTypeInodes == CapacityPool_NORMAL)
            normalPoolInodesMinMax.enter(freeInodes);
         else
         if(poolTypeInodes == CapacityPool_LOW)
            lowPoolInodesMinMax.enter(freeInodes);

         ++capacityInfoIter;
      }
   }

   if (pools->getDynamicPoolsEnabled() )
   {
      DemotionFlags demotionFlags(pools->getPoolLimitsSpace(), pools->getPoolLimitsInodes(),
         normalPoolFreeSpaceMinMax, lowPoolFreeSpaceMinMax,
         normalPoolInodesMinMax, lowPoolInodesMinMax);

      // Second loop: Iterate over nodes, check whether they have to be demoted
      if(demotionFlags.anyFlagActive() )
         demoteIfNecessary(pools, capacityInfos, demotionFlags);

      logDemotionFlags(demotionFlags, NODETYPE_Meta);
   }

   // Third loop: Assign nodes to pools.
   poolsModified |= assignNodesToPools(pools, capacityInfos);

   // if pool lists have changed, inform other nodes about it
   if((poolsModified) || (updateForced))
      updateMetaBuddyCapacityPools();

   if(poolsModified)
      Program::getApp()->getHeartbeatMgr()->notifyAsyncRefreshCapacityPools();
}

/**
 * Update capacity pool assignments for storage targets.
 *
 * @param updateForced true will force an update of the buddy mirror capacity pools.
 */
void InternodeSyncer::updateTargetCapacityPools(TargetCapacityPools* pools,
      TargetCapacityInfoList& targetCapacityInfos, bool updateForced)
{
   LogContext("Update Target CapPools").log(LogTopic_STATES, Log_DEBUG,
         "Starting target capacity pools update.");
   bool poolsModified = false; // true if pool lists changed and we need to inform others about it

   // Minima and maxima of the pools' free capacity - needed for adaptive pooling.
   MinMaxStore<int64_t> normalPoolFreeSpaceMinMax;
   MinMaxStore<int64_t> lowPoolFreeSpaceMinMax;
   MinMaxStore<int64_t> normalPoolInodesMinMax;
   MinMaxStore<int64_t> lowPoolInodesMinMax;

   {
      RWLockGuard const lock(targetCapacityReportMapLock, SafeRWLock_READ);

      // First loop: Look at all target infos and check free space.
      for (TargetCapacityInfoListIter targetCapacityInfoIter = targetCapacityInfos.begin();
           targetCapacityInfoIter != targetCapacityInfos.end(); /* iterator increment in loop */)
      {
         TargetCapacityInfo& targetCapacityInfo = *targetCapacityInfoIter;

         // Find target ID in map.
         TargetCapacityReportMapIter capacityReportIter =
            targetCapacityReportMap.find(targetCapacityInfo.getTargetID() );
         if (capacityReportIter == targetCapacityReportMap.end() )
         { // No known capacity report for the target. Either it's not (yet) reported, or timed out.
            // Carefully move this target to the emergency pool to at least avoid new allocations on
            // this target when other targets are still available.
            bool updateRes = assignTargetToPool(pools, targetCapacityInfo.getTargetID(),
               targetCapacityInfo.getNodeID(), CapacityPool_EMERGENCY,
               "No capacity report received." );
            poolsModified |= updateRes;

            // Remove target from list (as it is now assigned to a pool).
            targetCapacityInfoIter = targetCapacityInfos.erase(targetCapacityInfoIter);
            continue;
         }

         TargetCapacityReport& capacityReport = capacityReportIter->second;
         int64_t freeSpace = capacityReport.diskSpaceFree;
         int64_t freeInodes = capacityReport.inodesFree;

         CapacityPoolType poolTypeFreeSpace =
            pools->getPoolLimitsSpace().getPoolTypeFromFreeCapacity(freeSpace);
         CapacityPoolType poolTypeInodes =
            pools->getPoolLimitsInodes().getPoolTypeFromFreeCapacity(freeInodes);

         targetCapacityInfo.setFreeSpace(freeSpace);
         targetCapacityInfo.updateTargetPoolType(poolTypeFreeSpace);
         targetCapacityInfo.setFreeInodes(freeInodes);
         targetCapacityInfo.updateTargetPoolType(poolTypeInodes);

         // Store minimum / maximum for each pool.
         if (poolTypeFreeSpace == CapacityPool_NORMAL)
            normalPoolFreeSpaceMinMax.enter(freeSpace);
         else
         if (poolTypeFreeSpace == CapacityPool_LOW)
            lowPoolFreeSpaceMinMax.enter(freeSpace);

         if (poolTypeInodes == CapacityPool_NORMAL)
            normalPoolInodesMinMax.enter(freeInodes);
         else
         if (poolTypeInodes == CapacityPool_LOW)
            lowPoolInodesMinMax.enter(freeInodes);

         ++targetCapacityInfoIter;
      }
   }

   // Handle dynamic pool assignments (for targets that were not assigned to fixed pools above).

   if (pools->getDynamicPoolsEnabled() )
   {
      DemotionFlags demotionFlags(pools->getPoolLimitsSpace(), pools->getPoolLimitsInodes(),
         normalPoolFreeSpaceMinMax, lowPoolFreeSpaceMinMax,
         normalPoolInodesMinMax, lowPoolInodesMinMax);

      // Second loop: Iterate over targets, check whether they have to be demoted.
      if (pools->getDynamicPoolsEnabled() && demotionFlags.anyFlagActive() )
         demoteIfNecessary(pools, targetCapacityInfos, demotionFlags);

      logDemotionFlags(demotionFlags, NODETYPE_Storage);
   }

   // Third loop: Assign targets to pools.
   poolsModified |= assignTargetsToPools(pools, targetCapacityInfos);

   if (poolsModified || updateForced)
      updateTargetBuddyCapacityPools();

   // If pool lists have changed, inform other nodes about it.
   if (poolsModified)
      Program::getApp()->getHeartbeatMgr()->notifyAsyncRefreshCapacityPools();
}

/**
 * Assign the targets in the list to their designated storage capacity pools.
 * @returns True if one or more targets have been assigned to storage pools they were not in before.
 */
bool InternodeSyncer::assignTargetsToPools(TargetCapacityPools* pools,
   const TargetCapacityInfoList& targetCapacityInfos)
{
   bool poolsModified = false;

   for(TargetCapacityInfoListConstIter targetCapacityInfoIter = targetCapacityInfos.begin();
       targetCapacityInfoIter != targetCapacityInfos.end(); ++targetCapacityInfoIter)
   {
      const TargetCapacityInfo& capacityInfo = *targetCapacityInfoIter;
      bool updateRes = assignTargetToPool(pools, capacityInfo.getTargetID(),
         capacityInfo.getNodeID(), capacityInfo.getTargetPool(), "Free capacity threshold");
      poolsModified |= updateRes;
   }

   return poolsModified;
}

/**
 * Assign the (metadata) nodes in the list to their designated storage capacity pools.
 * @returns True if one or more targets have been assigned to storage pools they were not in before.
 */
bool InternodeSyncer::assignNodesToPools(NodeCapacityPools* pools,
   const CapacityInfoList& capacityInfos)
{
   bool poolsModified = false;

   for(CapacityInfoListConstIter capacityInfoIter = capacityInfos.begin();
       capacityInfoIter != capacityInfos.end(); ++capacityInfoIter)
   {
      const CapacityInfo& capacityInfo = *capacityInfoIter;
      bool updateRes = assignNodeToPool(pools, capacityInfo.getNodeID(),
         capacityInfo.getTargetPool(), "Free capacity threshold");
      poolsModified |= updateRes;
   }

   return poolsModified;
}

void InternodeSyncer::getTargetMappingAsInfoList(TargetMapper* targetMapper,
   TargetCapacityInfoList& outTargetCapacityInfos)
{
   UInt16List targetIDs;
   NumNodeIDList nodeIDs;

   targetMapper->getMappingAsLists(targetIDs, nodeIDs);

   for (ZipConstIterRange<UInt16List, NumNodeIDList> iter(targetIDs, nodeIDs);
        !iter.empty(); ++iter)
      outTargetCapacityInfos.push_back(TargetCapacityInfo(*iter()->first, *iter()->second) );
}

std::map<StoragePoolId,TargetCapacityInfoList> InternodeSyncer::getTargetMappingAsGroupedInfoLists()
{
   const App* app = Program::getApp();
   const TargetMapper* targetMapper = app->getTargetMapper();

   UInt16List targetIDs;
   NumNodeIDList nodeIDs;
   std::map<StoragePoolId, TargetCapacityInfoList> targetCapacityInfos;

   targetMapper->getMappingAsLists(targetIDs, nodeIDs);

   for (ZipConstIterRange<UInt16List, NumNodeIDList> iter(targetIDs, nodeIDs);
        !iter.empty(); ++iter)
   {
      const uint16_t targetId = *iter()->first;
      const NumNodeID nodeId = *iter()->second;
      TargetCapacityInfo info(targetId, nodeId);

      const StoragePoolExPtr storagePool = app->getStoragePoolStore()->getPool(targetId);
      if(!storagePool)
      {
         LOG(GENERAL, ERR, "TargetMapper has target, which is not assigned to a storage pool.",
             targetId);
         continue;
      }

      targetCapacityInfos[storagePool->getId()].push_back(info);
   }

   return targetCapacityInfos;
}

void InternodeSyncer::getNodesAsInfoList(CapacityInfoList& outCapacityInfos)
{
   App* app = Program::getApp();
   NodeStoreServersEx* metaNodes = app->getMetaNodes();

   auto metaNodeList = metaNodes->referenceAllNodes();

   for (auto nodeIt = metaNodeList.begin(); nodeIt != metaNodeList.end(); ++nodeIt)
      outCapacityInfos.push_back( CapacityInfo( (*nodeIt)->getNumID() ) );
}

/**
 * Wrapper for pools->addOrUpdate (storage target ID version) which also produces a log message.
 * @param nodeIDStr node ID as a string for logging
 * @param reason reason the target has been put into that pool (for logging)
 * @returns true if the target has been assigned to the pool, false if it already was in that pool.
 */
bool InternodeSyncer::assignTargetToPool(TargetCapacityPools* pools, uint16_t targetID,
   NumNodeID nodeID, CapacityPoolType pool, const std::string& reason)
{
   const char* logContext = "Assign target to capacity pool";

   bool updateRes = pools->addOrUpdate(targetID, nodeID, pool);
   if(updateRes)
      LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
         "Storage target capacity pool assignment updated. "
         "NodeID: " + nodeID.str() + "; "
         "TargetID: " + StringTk::uintToStr(targetID) + "; "
         "Pool: " + TargetCapacityPools::poolTypeToStr(pool) +
         ( (pool == CapacityPool_NORMAL) ? "." : ";  Reason: " + reason) );

   return updateRes;
}

/**
 * Wrapper for pools->addOrUpdate (metadata node version) which also produces a log message.
 * @param nodeIDStr node ID as a string for logging
 * @param reason reason the target has been put into that pool (for logging)
 * @returns true if the target has been assigned to the pool, false if it already was in that pool.
 */
bool InternodeSyncer::assignNodeToPool(NodeCapacityPools* pools, NumNodeID nodeID,
   CapacityPoolType pool, const std::string& reason)
{
   const char* logContext = "Assign node to capacity pool";

   bool updateRes = pools->addOrUpdate(nodeID.val(), pool);
   if(updateRes)
      LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
         "Metadata node capacity pool assignment updated. "
         "NodeID: " + nodeID.str() + "; "
         "Pool: " + NodeCapacityPools::poolTypeToStr(pool) + "; "
         "Reason: " + reason);

   return updateRes;
}

/**
 * Check if demotion was enabled for a pool, and check (storage) targets' / (metadata) nodes'
 * free space / inodes and assign them to a lower pool if necessary.
 */
template <typename P, typename L>
void InternodeSyncer::demoteIfNecessary(P* pools, L& capacityInfos,
   const DemotionFlags& demotionFlags)
{
   for(typename L::iterator capacityInfoIter = capacityInfos.begin();
      capacityInfoIter != capacityInfos.end(); ++capacityInfoIter)
   {
      typename L::value_type& capacityInfo = *capacityInfoIter;
      CapacityPoolType pool = capacityInfo.getTargetPool();

      if(pool == CapacityPool_NORMAL)
      {
         const bool demoteNormalSpace =
            pools->getPoolLimitsSpace().demoteNormalToLow(capacityInfo.getFreeSpace() );
         const bool demoteNormalInodes =
            pools->getPoolLimitsInodes().demoteNormalToLow(capacityInfo.getFreeInodes() );

         if ( ( demotionFlags.getNormalPoolSpaceFlag() && demoteNormalSpace )
            || ( demotionFlags.getNormalPoolInodesFlag() && demoteNormalInodes ) )
         {
            capacityInfo.setTargetPoolType(CapacityPool_LOW);
         }
      }
      else if(pool == CapacityPool_LOW)
      {
         const bool demoteLowSpace =
            pools->getPoolLimitsSpace().demoteLowToEmergency(capacityInfo.getFreeSpace() );
         const bool demoteLowInodes =
            pools->getPoolLimitsInodes().demoteLowToEmergency(
               capacityInfo.getFreeInodes() );
         if ( ( demotionFlags.getLowPoolSpaceFlag() && demoteLowSpace )
            || ( demotionFlags.getLowPoolInodesFlag() && demoteLowInodes ) )
         {
            capacityInfo.setTargetPoolType(CapacityPool_EMERGENCY);
         }
      }
   }
}

void InternodeSyncer::logDemotionFlags(const DemotionFlags& demotionFlags, NodeType nodeType)
{
   static DemotionFlags previousFlagsStorage;
   static DemotionFlags previousFlagsMeta;

   if (  ( (nodeType == NODETYPE_Storage) && (previousFlagsStorage != demotionFlags) )
      || ( (nodeType == NODETYPE_Meta)    && (previousFlagsMeta != demotionFlags) ) )
   {
      LogContext("Demote targets (" + boost::lexical_cast<std::string>(nodeType) + ")").log(Log_NOTICE,
         "demotion NORMAL->LOW [free space]: "
         + std::string(demotionFlags.getNormalPoolSpaceFlag() ? "on" : "off") +
         " [inodes]: "
         + std::string(demotionFlags.getNormalPoolInodesFlag() ? "on" : "off") +
         "; demotion LOW->EMERGENCY [free space]: "
         + std::string(demotionFlags.getLowPoolSpaceFlag() ? "on" : "off") +
         " [inodes]: "
         + std::string(demotionFlags.getLowPoolInodesFlag() ? "on" : "off") );

      if (nodeType == NODETYPE_Storage)
         previousFlagsStorage = demotionFlags;
      else
         previousFlagsMeta = demotionFlags;
   }
}

void InternodeSyncer::updateMetaBuddyCapacityPools()
{
   App* app = Program::getApp();

   MirrorBuddyGroupMapper* buddyMapper = app->getMetaBuddyGroupMapper();
   NodeCapacityPools* metaPools = app->getMetaCapacityPools();
   NodeCapacityPools* buddyPools = app->getMetaBuddyCapacityPools();

   UInt16SetVector newCapacityPools(CapacityPool_END_DONTUSE);

   UInt16List buddyGroupIDs;
   MirrorBuddyGroupList buddyGroups;

   buddyMapper->getMappingAsLists(buddyGroupIDs, buddyGroups);

   UInt16ListConstIter buddyGroupIDIter = buddyGroupIDs.begin();
   MirrorBuddyGroupListCIter buddyGroupIter = buddyGroups.begin();

   // walk all mapped buddy groups, add them to buddy pools based on worst buddy pool of any buddy
   for( ; buddyGroupIDIter != buddyGroupIDs.end(); buddyGroupIDIter++, buddyGroupIter++)
   {
      CapacityPoolType primaryPoolType;
      bool primaryFound = metaPools->getPoolAssignment(buddyGroupIter->firstTargetID,
         &primaryPoolType);

      if(!primaryFound)
         continue; // can't make a pool decision if a buddy does not exist

      CapacityPoolType secondaryPoolType;
      bool secondaryFound = metaPools->getPoolAssignment(buddyGroupIter->secondTargetID,
         &secondaryPoolType);

      if(!secondaryFound)
         continue; // can't make a pool decision if a buddy does not exist

      CapacityPoolType groupPoolType = BEEGFS_MAX(primaryPoolType, secondaryPoolType);

      newCapacityPools[groupPoolType].insert(*buddyGroupIDIter);
   }

   // apply new pools
   buddyPools->syncPoolsFromSets(newCapacityPools);
}

void InternodeSyncer::updateTargetBuddyCapacityPools()
{
   const App* app = Program::getApp();

   const MirrorBuddyGroupMapper* buddyMapper = app->getStorageBuddyGroupMapper();
   const StoragePoolStore* storagePoolStore = app->getStoragePoolStore();
   const StoragePoolPtrVec storagePools = storagePoolStore->getPoolsAsVec();

   for (auto poolIter = storagePools.begin(); poolIter != storagePools.end(); poolIter++)
   {
      TargetCapacityPools* targetPools = (*poolIter)->getTargetCapacityPools();
      NodeCapacityPools* buddyPools = (*poolIter)->getBuddyCapacityPools();
      const UInt16Set buddyGroupIds = (*poolIter)->getBuddyGroups();

      UInt16SetVector newCapacityPools(CapacityPool_END_DONTUSE);

      // walk all mapped buddy groups, add them to buddy pools based on worst buddy pool of buddies
      for (auto buddyIter = buddyGroupIds.begin(); buddyIter != buddyGroupIds.end(); buddyIter++)
      {
         const MirrorBuddyGroup mbg = buddyMapper->getMirrorBuddyGroup(*buddyIter);

         CapacityPoolType primaryPoolType;
         const bool primaryFound = targetPools->getPoolAssignment(mbg.firstTargetID,
                                                                  &primaryPoolType);

         if(!primaryFound)
            continue; // can't make a pool decision if a buddy does not exist

         CapacityPoolType secondaryPoolType;
         const bool secondaryFound = targetPools->getPoolAssignment(mbg.secondTargetID,
                                                                    &secondaryPoolType);

         if(!secondaryFound)
            continue; // can't make a pool decision if a buddy does not exist

         const CapacityPoolType groupPoolType = BEEGFS_MAX(primaryPoolType, secondaryPoolType);

         newCapacityPools[groupPoolType].insert(*buddyIter);
      }

      // apply new pools
      buddyPools->syncPoolsFromSets(newCapacityPools);
   }
}


void InternodeSyncer::saveStates()
{
   App* app = Program::getApp();
   auto* targetStateStore = app->getTargetStateStore();
   auto* metaStateStore = app->getMetaStateStore();

   targetStateStore->saveStatesToFile();
   metaStateStore->saveStatesToFile();
}

/**
 * Saves targetStrID->numID mappings (from targetNumIDMapper) and target->node mappings (from
 * targetMapper).
 */
void InternodeSyncer::saveTargetMappings()
{
   App* app = Program::getApp();
   NumericIDMapper* targetNumIDMapper = app->getTargetNumIDMapper();
   auto targetMapper = app->getTargetMapper();
   auto storageMirrorGroupMapper = app->getStorageBuddyGroupMapper();
   auto metaMirrorGroupMapper = app->getMetaBuddyGroupMapper();
   auto storagePoolStore = app->getStoragePoolStore();

   targetNumIDMapper->saveToFile();
   targetMapper->saveToFile();
   storageMirrorGroupMapper->saveToFile();
   metaMirrorGroupMapper->saveToFile();
   storagePoolStore->saveToFile();
}

/**
 * Drop/reset idle conns from all server stores.
 */
void InternodeSyncer::dropIdleConns()
{
   App* app = Program::getApp();

   unsigned numDroppedConns = 0;

   numDroppedConns += dropIdleConnsByStore(app->getMgmtNodes() );
   numDroppedConns += dropIdleConnsByStore(app->getMetaNodes() );
   numDroppedConns += dropIdleConnsByStore(app->getStorageNodes() );

   if(numDroppedConns)
   {
      log.log(Log_DEBUG, "Dropped idle connections: " + StringTk::uintToStr(numDroppedConns) );
   }
}

/**
 * Walk over all nodes in the given store and drop/reset idle connections.
 *
 * @return number of dropped connections
 */
unsigned InternodeSyncer::dropIdleConnsByStore(const NodeStoreServersEx* nodes)
{
   App* app = Program::getApp();

   unsigned numDroppedConns = 0;

   for (const auto& node : nodes->referenceAllNodes())
   {
      /* don't do any idle disconnect stuff with local node
         (the LocalNodeConnPool doesn't support and doesn't need this kind of treatment) */

      if (node.get() != &app->getLocalNode())
      {
         NodeConnPool* connPool = node->getConnPool();

         numDroppedConns += connPool->disconnectAndResetIdleStreams();
      }
   }

   return numDroppedConns;
}

/**
 * Clears out stale capacity reports, i.e. reports where the target/node is not ONLINE+GOOD
 * or where the target node is unknown to the targetStateStore (has been unmapped in the
 * meantime) from the target/nodeCapacityReportMap.
 *
 * @param nodeType Selects whether to look at storage targets or metadata nodes.
 */
void InternodeSyncer::clearStaleCapacityReports(const NodeType nodeType)
{
   TargetStateStore* stateStore = (nodeType == NODETYPE_Meta)
      ? Program::getApp()->getMetaStateStore()
      : Program::getApp()->getTargetStateStore();

   RWLock* reportsRWLock = (nodeType == NODETYPE_Meta)
      ? &nodeCapacityReportMapLock
      : &targetCapacityReportMapLock;

   TargetCapacityReportMap& capacityReportMap = (nodeType == NODETYPE_Meta)
      ? nodeCapacityReportMap
      : targetCapacityReportMap;

   const CombinedTargetState onlineGoodState(
      TargetReachabilityState_ONLINE, TargetConsistencyState_GOOD);

   RWLockGuard const lock(*reportsRWLock, SafeRWLock_WRITE);

   for (TargetCapacityReportMapIter reportIter = capacityReportMap.begin();
        reportIter != capacityReportMap.end(); /* iter increment in loop body */ )
   {
      const uint16_t targetID = reportIter->first;

      CombinedTargetState targetState;

      bool getStateRes = stateStore->getState(targetID, targetState);

      if ( !getStateRes || (targetState != onlineGoodState) )
         capacityReportMap.erase(reportIter++);
      else
         ++reportIter;
   }
}
