#include <app/config/Config.h>
#include <app/App.h>
#include <common/net/message/nodes/ChangeTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/ChangeTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/MapTargetsMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/storage/GetStorageTargetInfoMsg.h>
#include <common/net/message/storage/GetStorageTargetInfoRespMsg.h>
#include <common/net/message/storage/SetStorageTargetInfoMsg.h>
#include <common/net/message/storage/SetStorageTargetInfoRespMsg.h>
#include <common/net/message/storage/quota/RequestExceededQuotaMsg.h>
#include <common/net/message/storage/quota/RequestExceededQuotaRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/ZipIterator.h>
#include <common/nodes/NodeStore.h>
#include <common/nodes/TargetCapacityPools.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <program/Program.h>

#include "InternodeSyncer.h"

InternodeSyncer::InternodeSyncer() throw(ComponentInitException) : PThread("XNodeSync"),
   log("XNodeSync"), forceTargetStatesUpdate(true), forcePublishCapacities(true)
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
}

void InternodeSyncer::syncLoop()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   const int sleepIntervalMS = 3*1000; // 3sec

   const unsigned sweepNormalMS = 5*1000; // 5sec
   const unsigned sweepStressedMS = 2*1000; // 2sec
   const unsigned idleDisconnectIntervalMS = 70*60*1000; /* 70 minutes (must be less than half the
      streamlis idle disconnect interval to avoid cases where streamlis disconnects first) */
   const unsigned downloadNodesIntervalMS = 600000; // 10min

   // If (undocumented) sysUpdateTargetStatesSecs is set in config, use that value, otherwise
   // default to 1/6 sysTargetOfflineTimeoutSecs.
   const unsigned updateTargetStatesMS =
      (cfg->getSysUpdateTargetStatesSecs() != 0)
      ? cfg->getSysUpdateTargetStatesSecs() * 1000
      : cfg->getSysTargetOfflineTimeoutSecs() * 166;

   const unsigned updateCapacitiesMS = updateTargetStatesMS * 4;

   Time lastCacheSweepT;
   Time lastIdleDisconnectT;
   Time lastDownloadNodesT;
   Time lastTargetStatesUpdateT;
   Time lastCapacityUpdateT;

   unsigned currentCacheSweepMS = sweepNormalMS; // (adapted inside the loop below)

   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      bool targetStatesUpdateForced = getAndResetForceTargetStatesUpdate();
      bool publishCapacitiesForced = getAndResetForcePublishCapacities();

      if(lastCacheSweepT.elapsedMS() > currentCacheSweepMS)
      {
         bool flushTriggered = app->getChunkDirStore()->cacheSweepAsync();
         currentCacheSweepMS = (flushTriggered ? sweepStressedMS : sweepNormalMS);

         lastCacheSweepT.setToNow();
      }

      if(lastIdleDisconnectT.elapsedMS() > idleDisconnectIntervalMS)
      {
         dropIdleConns();
         lastIdleDisconnectT.setToNow();
      }

      // download & sync nodes
      if(lastDownloadNodesT.elapsedMS() > downloadNodesIntervalMS)
      {
         downloadAndSyncNodes();
         downloadAndSyncTargetMappings();
         downloadAndSyncMirrorBuddyGroups();

         lastDownloadNodesT.setToNow();
      }

      if( targetStatesUpdateForced ||
          (lastTargetStatesUpdateT.elapsedMS() > updateTargetStatesMS) )
      {
         updateTargetStatesAndBuddyGroups();
         lastTargetStatesUpdateT.setToNow();
      }

      if( publishCapacitiesForced ||
          (lastCapacityUpdateT.elapsedMS() > updateCapacitiesMS))
      {
         publishTargetCapacities();
         lastCapacityUpdateT.setToNow();
      }
   }
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
unsigned InternodeSyncer::dropIdleConnsByStore(NodeStoreServersEx* nodes)
{
   App* app = Program::getApp();

   unsigned numDroppedConns = 0;

   auto node = nodes->referenceFirstNode();
   while(node)
   {
      /* don't do any idle disconnect stuff with local node
         (the LocalNodeConnPool doesn't support and doesn't need this kind of treatment) */

      if (node.get() != &app->getLocalNode())
      {
         NodeConnPool* connPool = node->getConnPool();

         numDroppedConns += connPool->disconnectAndResetIdleStreams();
      }

      node = nodes->referenceNextNode(node); // iterate to next node
   }

   return numDroppedConns;
}

void InternodeSyncer::updateTargetStatesAndBuddyGroups()
{
   const char* logContext = "Update states and mirror groups";
   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG,
         "Starting target state update.");

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   TargetStateStore* targetStateStore = app->getTargetStateStore();
   StorageTargets* storageTargets = app->getStorageTargets();
   MirrorBuddyGroupMapper* mirrorBuddyGroupMapper = app->getMirrorBuddyGroupMapper();

   static bool downloadFailedLogged = false; // to avoid log spamming
   static bool publishFailedLogged = false; // to avoid log spamming

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (unlikely(!mgmtNode))
   { // should never happen here, because mgmt is downloaded before InternodeSyncer startup
      LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR, "Management node not defined.");
      return;
   }

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   UInt16List targetIDs;
   UInt8List targetReachabilityStates;
   UInt8List targetConsistencyStates;

   unsigned numRetries = 10; // If publishing states fails 10 times, give up (-> POFFLINE).

   // Note: Publishing states fails if between downloadStatesAndBuddyGroups and
   // publishLocalTargetStateChanges, a state on the mgmtd is changed (e.g. because the primary
   // sets NEEDS_RESYNC for the secondary). In that case, we will retry.

   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG,
         "Beginning target state update...");
   bool publishSuccess = false;

   while (!publishSuccess && (numRetries--) )
   {
      // Clear all the lists in case we are already retrying and the lists contain leftover data
      // from the previous attempt.
      buddyGroupIDs.clear();
      primaryTargetIDs.clear();
      secondaryTargetIDs.clear();
      targetIDs.clear();
      targetReachabilityStates.clear();
      targetConsistencyStates.clear();

      bool downloadRes = NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Storage,
         &buddyGroupIDs, &primaryTargetIDs, &secondaryTargetIDs, &targetIDs,
         &targetReachabilityStates, &targetConsistencyStates, true);

      if (!downloadRes)
      {
         if(!downloadFailedLogged)
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
               "Downloading target states from management node failed. "
               "Setting all targets to probably-offline.");
            downloadFailedLogged = true;
         }

         targetStateStore->setAllStates(TargetReachabilityState_POFFLINE);

         break;
      }

      downloadFailedLogged = false;

      // Store old states for ChangeTargetConsistencyStatesMsg.
      UInt8List oldConsistencyStates = targetConsistencyStates;

      // Sync buddy groups here, because decideResync depends on it.
      // This is not a problem because if pushing target states fails all targets will be
      // (p)offline anyway.
      targetStateStore->syncStatesAndGroupsFromLists(mirrorBuddyGroupMapper,
         targetIDs, targetReachabilityStates, targetConsistencyStates,
         buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs, app->getLocalNode().getNumID());

      storageTargets->syncBuddyGroupMap(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs);

      TargetStateMap statesFromMgmtd;
      StorageTargets::fillTargetStateMap(targetIDs, targetReachabilityStates,
         targetConsistencyStates, statesFromMgmtd);

      TargetStateMap localTargetChangedStates;
      storageTargets->decideResync(statesFromMgmtd, localTargetChangedStates);

      StorageTargets::updateTargetStateLists(localTargetChangedStates, targetIDs,
         targetReachabilityStates, targetConsistencyStates);

      publishSuccess = publishLocalTargetStateChanges(
         targetIDs, oldConsistencyStates, targetConsistencyStates);

      if(publishSuccess)
         storageTargets->checkBuddyNeedsResync();
   }

   if(!publishSuccess)
   {
      if(!publishFailedLogged)
      {
         log.log(LogTopic_STATESYNC, Log_WARNING,
               "Pushing local target states to management node failed.");
         publishFailedLogged = true;
      }
   }
   else
      publishFailedLogged = false;
}

void InternodeSyncer::publishTargetCapacities()
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   StorageTargets* storageTargets = app->getStorageTargets();

   log.log(LogTopic_STATESYNC, Log_DEBUG, "Publishing target capacity infos.");

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      log.log(LogTopic_STATESYNC, Log_ERR, "Management node not defined.");
      return;
   }

   UInt16List targetIDs;
   StorageTargetInfoList targetInfoList;

   storageTargets->getAllTargetIDs(&targetIDs);
   storageTargets->generateTargetInfoList(targetIDs, targetInfoList);

   SetStorageTargetInfoMsg msg(NODETYPE_Storage, &targetInfoList);
   RequestResponseArgs rrArgs(mgmtNode.get(), &msg, NETMSGTYPE_SetStorageTargetInfoResp);

#ifndef BEEGFS_DEBUG
   rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                    | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif

   bool sendRes = MessagingTk::requestResponse(&rrArgs);

   static bool failureLogged = false;

   if (!sendRes)
   {
      if (!failureLogged)
         log.log(LogTopic_STATESYNC, Log_CRITICAL,
               "Pushing target free space info to management node failed.");

      failureLogged = true;
      return;
   }
   else
   {
      SetStorageTargetInfoRespMsg* respMsgCast =
         static_cast<SetStorageTargetInfoRespMsg*>(rrArgs.outRespMsg);

      failureLogged = false;

      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
      {
         log.log(LogTopic_STATESYNC, Log_CRITICAL,
               "Management did not accept target free space info message.");
         return;
      }
   }

   // If we were just started and are publishing our capacity for the first time, force a pool
   // refresh on the mgmtd so we're not stuck in the emergency pool until the first regular
   // pool refresh.
   static bool firstTimePubilished = true;
   if (firstTimePubilished)
   {
      forceMgmtdPoolsRefresh();
      firstTimePubilished = false;
   }
}

void InternodeSyncer::publishTargetState(uint16_t targetID, TargetConsistencyState targetState)
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();

   log.log(Log_DEBUG, "Publishing state for target: " + StringTk::uintToStr(targetID) );

   UInt16List targetIDs(1, targetID);
   UInt8List states(1, targetState);

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      log.logErr("Management node not defined.");
      return;
   }

   SetTargetConsistencyStatesMsg msg(NODETYPE_Storage, &targetIDs, &states, true);

   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   bool sendRes = MessagingTk::requestResponse(*mgmtNode, &msg,
      NETMSGTYPE_SetTargetConsistencyStatesResp, &respBuf, &respMsg);

   if (!sendRes)
      log.log(Log_CRITICAL, "Pushing target state to management node failed.");
   else
   {
      SetTargetConsistencyStatesRespMsg* respMsgCast = (SetTargetConsistencyStatesRespMsg*)respMsg;
      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
         log.log(Log_CRITICAL, "Management node did not accept target state.");

      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);
   }
}

/**
 * Gets a list of target states changes (old/new), and reports the local ones (targets which are
 * present in this storage server's storageTargetDataMap) to the mgmtd.
 */
bool InternodeSyncer::publishLocalTargetStateChanges(UInt16List& targetIDs, UInt8List& oldStates,
      UInt8List& newStates)
{
   App* app = Program::getApp();
   StorageTargets* storageTargets = app->getStorageTargets();
   TargetOfflineWait& targetOfflineWait = storageTargets->getTargetOfflineWait();
   bool someTargetHasOfflineTimeout = targetOfflineWait.anyTargetHasTimeout();

   UInt16List localTargetIDs;
   UInt8List localOldStates;
   UInt8List localNewStates;

   for (ZipIterRange<UInt16List, UInt8List, UInt8List>
        oldNewStatesIter(targetIDs, oldStates, newStates);
        !oldNewStatesIter.empty(); ++oldNewStatesIter)
   {
      uint16_t targetID = *(oldNewStatesIter()->first);

      if ( !storageTargets->isLocalTarget(targetID) )
         continue;

      // Don't report targets which have an offline timeout at the moment.
      if (someTargetHasOfflineTimeout && targetOfflineWait.targetHasTimeout(targetID) )
         continue;

      localTargetIDs.push_back(targetID);
      localOldStates.push_back(*(oldNewStatesIter()->second));
      localNewStates.push_back(*(oldNewStatesIter()->third));
   }

   return publishTargetStateChanges(localTargetIDs, localOldStates, localNewStates);
}

/**
 * Send a HeartbeatMsg to mgmt.
 *
 * @return true if node and targets registration successful
 */
bool InternodeSyncer::registerNode(AbstractDatagramListener* dgramLis)
{
   static bool registrationFailureLogged = false; // to avoid log spamming

   const char* logContext = "Register node";

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   Node& localNode = app->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NicAddressList nicList(localNode.getNicList() );
   const BitStore* nodeFeatureFlags = localNode.getNodeFeatures();

   HeartbeatMsg msg(localNode.getID(), localNodeNumID, NODETYPE_Storage, &nicList,
      nodeFeatureFlags);
   msg.setPorts(cfg->getConnStoragePortUDP(), cfg->getConnStoragePortTCP() );
   msg.setFhgfsVersion(BEEGFS_VERSION_CODE);

   bool registered = dgramLis->sendToNodeUDPwithAck(mgmtNode, &msg);

   if(registered)
      LogContext(logContext).log(Log_WARNING, "Node registration successful.");
   else
   if(!registrationFailureLogged)
   {
      LogContext(logContext).log(Log_CRITICAL, "Node registration not successful. "
         "Management node offline? Will keep on trying...");
      registrationFailureLogged = true;
   }

   return registered;
}

/**
 * Sent a MapTargetsMsg to mgmt.
 *
 * @return true if targets mapping successful.
 */
bool InternodeSyncer::registerTargetMappings()
{
   static bool registrationFailureLogged = false; // to avoid log spamming

   const char* logContext = "Register target mappings";

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   bool registered = false;

   Node& localNode = app->getLocalNode();
   NumNodeID localNodeID = localNode.getNumID();
   StorageTargets* targets = Program::getApp()->getStorageTargets();
   UInt16List targetIDs;

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   MapTargetsRespMsg* respMsgCast;

   targets->getAllTargetIDs(&targetIDs);

   MapTargetsMsg msg(&targetIDs, localNodeID);


   // connect & communicate
   commRes = MessagingTk::requestResponse(
      *mgmtNode, &msg, NETMSGTYPE_MapTargetsResp, &respBuf, &respMsg);
   if(commRes)
   {
      // handle result
      respMsgCast = (MapTargetsRespMsg*)respMsg;

      FhgfsOpsErr serverErr = (FhgfsOpsErr)respMsgCast->getValue();
      if(serverErr != FhgfsOpsErr_SUCCESS)
      {
         if(!registrationFailureLogged)
         {
            LogContext(logContext).log(Log_CRITICAL, "Storage targets registration rejected. "
               "Will keep on trying... "
               "(Server Error: " + std::string(FhgfsOpsErrTk::toErrString(serverErr) ) + ")");
            registrationFailureLogged = true;
         }
      }
      else
         registered = true;

      // cleanup
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);
   }

   if(registered)
      LogContext(logContext).log(Log_WARNING, "Storage targets registration successful.");
   else
   if(!registrationFailureLogged)
   {
      LogContext(logContext).log(Log_CRITICAL, "Storage targets registration not successful. "
         "Management node offline? Will keep on trying...");
      registrationFailureLogged = true;
   }

   return registered;
}

bool InternodeSyncer::publishTargetStateChanges(UInt16List& targetIDs, UInt8List& oldStates,
   UInt8List& newStates)
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   bool res;

   log.log(Log_DEBUG, "Publishing target state change");

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      log.logErr("Management node not defined.");
      return true; // Don't stall indefinitely if we don't have a management node.
   }

   ChangeTargetConsistencyStatesMsg msg(NODETYPE_Storage, &targetIDs, &oldStates, &newStates);

   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   bool sendRes = MessagingTk::requestResponse(*mgmtNode, &msg,
      NETMSGTYPE_ChangeTargetConsistencyStatesResp, &respBuf, &respMsg);

   if (!sendRes)
   {
      log.log(Log_CRITICAL, "Pushing target state changes to management node failed.");
      res = false; // Retry.
   }
   else
   {
      ChangeTargetConsistencyStatesRespMsg* respMsgCast =
         (ChangeTargetConsistencyStatesRespMsg*)respMsg;

      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
      {
         log.log(Log_CRITICAL, "Management node did not accept target state changes.");
         res = false; // States were changed while we evaluated the state changed. Try again.
      }
      else
         res = true;

      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);
   }

   return res;
}

void InternodeSyncer::requestBuddyTargetStates()
{
   const char* logContext = "Request buddy target states";

   TimerQueue* timerQ = Program::getApp()->getTimerQueue();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   MirrorBuddyGroupMapper* buddyGroupMapper = Program::getApp()->getMirrorBuddyGroupMapper();
   StorageTargets* storageTargets = Program::getApp()->getStorageTargets();
   NodeStore* storageNodes = Program::getApp()->getStorageNodes();
   TargetStateStore* targetStateStore = Program::getApp()->getTargetStateStore();
   UInt16List localStorageTargetIDs;
   const StorageTargetInfoList* storageTargetInfoList;

   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG, "Requesting buddy target states.");

   storageTargets->getAllTargetIDs(&localStorageTargetIDs);

   // loop over all local targets
   for(UInt16ListIter iter = localStorageTargetIDs.begin();
       iter != localStorageTargetIDs.end();
       iter++)
   {
      uint16_t targetID = *iter;
      TargetConsistencyState buddyTargetConsistencyState;
      bool buddyNeedsResync;

      // check if target is part of a buddy group
      uint16_t buddyTargetID = buddyGroupMapper->getBuddyTargetID(targetID);
      if(!buddyTargetID)
         continue;

      // this target is part of a buddy group

      NumNodeID nodeID = targetMapper->getNodeID(buddyTargetID);
      if(!nodeID)
      { // mapping to node not found
         LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
            "Node-mapping for target ID " + StringTk::uintToStr(buddyTargetID) + " not found.");
         continue;
      }

      auto node = storageNodes->referenceNode(nodeID);
      if(!node)
      { // node not found
         LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
            "Unknown storage node. nodeID: " + nodeID.str() + "; targetID: "
               + StringTk::uintToStr(targetID));
         continue;
      }

      // get reachability state of buddy target ID
      CombinedTargetState currentState;
      targetStateStore->getState(buddyTargetID, currentState);

      bool commRes;
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      GetStorageTargetInfoRespMsg* respMsgCast;

      if(currentState.reachabilityState == TargetReachabilityState_ONLINE)
      {
         // communicate
         UInt16List queryTargetIDs;
         queryTargetIDs.push_back(buddyTargetID);
         GetStorageTargetInfoMsg msg(&queryTargetIDs);

         // connect & communicate
         commRes = MessagingTk::requestResponse(*node, &msg,
         NETMSGTYPE_GetStorageTargetInfoResp, &respBuf, &respMsg);
         if(!commRes)
         { // communication failed
            LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
               "Communication with buddy target failed. "
                  "nodeID: " + nodeID.str() + "; buddy targetID: "
                  + StringTk::uintToStr(buddyTargetID));

            goto cleanup;
         }

         // handle response
         respMsgCast = (GetStorageTargetInfoRespMsg*) respMsg;
         storageTargetInfoList = &respMsgCast->getStorageTargetInfos();

         // get received target information
         // (note: we only requested a single target info, so the first one must be the
         // requested one)
         buddyTargetConsistencyState =
            storageTargetInfoList->empty() ? TargetConsistencyState_BAD :
               storageTargetInfoList->front().getState();

         // set last comm timestamp, but ignore it if we think buddy needs a resync
         buddyNeedsResync = storageTargets->getBuddyNeedsResync(targetID);
         if((buddyTargetConsistencyState == TargetConsistencyState_GOOD) && !buddyNeedsResync)
            storageTargets->writeLastBuddyCommTimestamp(targetID);
      }

      cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);
   }

   // requeue
   timerQ->enqueue(std::chrono::seconds(30), requestBuddyTargetStates);
}

/**
 * @param outTargetIDs
 * @param outReachabilityStates
 * @param outConsistencyStates
 * @return false on error.
 */
bool InternodeSyncer::downloadAndSyncTargetStates(UInt16List& outTargetIDs,
   UInt8List& outReachabilityStates, UInt8List& outConsistencyStates)
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   TargetStateStore* targetStateStore = app->getTargetStateStore();

   auto node = mgmtNodes->referenceFirstNode();
   if(!node)
      return false;

   bool downloadRes = NodesTk::downloadTargetStates(*node, NODETYPE_Storage,
      &outTargetIDs, &outReachabilityStates, &outConsistencyStates, false);

   if(downloadRes)
      targetStateStore->syncStatesFromLists(outTargetIDs, outReachabilityStates,
         outConsistencyStates);

   return downloadRes;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncNodes()
{
   const char* logContext = "Nodes sync";
   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG, "Called.");

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   Node& localNode = app->getLocalNode();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;


   { // storage nodes
      std::vector<NodeHandle> storageNodesList;
      NumNodeIDList addedStorageNodes;
      NumNodeIDList removedStorageNodes;

      bool storageRes =
         NodesTk::downloadNodes(*mgmtNode, NODETYPE_Storage, storageNodesList, true);
      if(!storageRes)
         goto err_release_mgmt;

      storageNodes->syncNodes(storageNodesList, &addedStorageNodes, &removedStorageNodes, true,
         &localNode);
      printSyncNodesResults(NODETYPE_Storage, &addedStorageNodes, &removedStorageNodes);
   }


   { // clients
      std::vector<NodeHandle> clientsList;
      StringList addedClients;
      StringList removedClients;

      bool clientsRes = NodesTk::downloadNodes(*mgmtNode, NODETYPE_Client, clientsList, true);
      if(!clientsRes)
         goto err_release_mgmt;

      // note: storage App doesn't have a client node store, thus no clients->syncNodes() here
      syncClientSessions(clientsList);
   }


   { // metadata nodes
      std::vector<NodeHandle> metaNodesList;
      NumNodeIDList addedMetaNodes;
      NumNodeIDList removedMetaNodes;
      NumNodeID rootNodeID;
      bool rootIsBuddyMirrored;

      bool metaRes =
         NodesTk::downloadNodes(*mgmtNode, NODETYPE_Meta, metaNodesList, true, &rootNodeID,
            &rootIsBuddyMirrored);
      if(!metaRes)
         goto err_release_mgmt;

      metaNodes->syncNodes(metaNodesList, &addedMetaNodes, &removedMetaNodes, true);

      if(metaNodes->setRootNodeNumID(rootNodeID, false, rootIsBuddyMirrored) )
      {
         LogContext(logContext).log(LogTopic_STATESYNC, Log_CRITICAL,
            "Root NodeID (from sync results): " + rootNodeID.str());
      }

      printSyncNodesResults(NODETYPE_Meta, &addedMetaNodes, &removedMetaNodes);
   }

   return true;

err_release_mgmt:
   return false;
}

void InternodeSyncer::printSyncNodesResults(NodeType nodeType, NumNodeIDList* addedNodes,
   NumNodeIDList* removedNodes)
{
   const char* logContext = "Sync results";

   if(addedNodes->size() )
      LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
         std::string("Nodes added: ") +
         StringTk::uintToStr(addedNodes->size() ) +
         " (Type: " + Node::nodeTypeToStr(nodeType) + ")");

   if(removedNodes->size() )
      LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
         std::string("Nodes removed: ") +
         StringTk::uintToStr(removedNodes->size() ) +
         " (Type: " + Node::nodeTypeToStr(nodeType) + ")");
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncTargetMappings()
{
   LogContext("Download target mappings").log(LogTopic_STATESYNC, Log_DEBUG,
         "Syncing target mappings.");

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   bool retVal = true;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   UInt16List targetIDs;
   NumNodeIDList nodeIDs;

   bool downloadRes = NodesTk::downloadTargetMappings(*mgmtNode, &targetIDs, &nodeIDs, true);
   if(downloadRes)
      targetMapper->syncTargetsFromLists(targetIDs, nodeIDs);
   else
      retVal = false;

   return retVal;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncMirrorBuddyGroups()
{
   LogContext("Downlod mirror groups").log(LogTopic_STATESYNC, Log_DEBUG,
         "Syncing mirror groups.");
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();
   StorageTargets* storageTargets = app->getStorageTargets();

   bool retVal = true;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   bool downloadRes = NodesTk::downloadMirrorBuddyGroups(*mgmtNode, NODETYPE_Storage,
         &buddyGroupIDs, &primaryTargetIDs, &secondaryTargetIDs, true);

   if(downloadRes)
   {
      buddyGroupMapper->syncGroupsFromLists(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs,
         app->getLocalNode().getNumID());
      storageTargets->syncBuddyGroupMap(buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs);
   }
   else
      retVal = false;

   return retVal;
}

/**
 * Synchronize local client sessions with registered clients from mgmt to release orphaned sessions.
 *
 * @param clientsList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 */
void InternodeSyncer::syncClientSessions(const std::vector<NodeHandle>& clientsList)
{
   const char* logContext = "Client sessions sync";
   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG, "Client session sync started.");

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();

   SessionList removedSessions;
   NumNodeIDList unremovableSessions;

   sessions->syncSessions(clientsList, &removedSessions, &unremovableSessions);

   // print sessions removal results (upfront)
   if(!removedSessions.empty() || !unremovableSessions.empty() )
   {
      std::ostringstream logMsgStream;
      logMsgStream << "Removing " << removedSessions.size() << " client sessions. ";

      if(unremovableSessions.empty()) // no unremovable sessions
         LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG, logMsgStream.str() );
      else
      { // unremovable sessions found => log warning
         logMsgStream << "(" << unremovableSessions.size() << " are unremovable)";
         LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING, logMsgStream.str() );
      }
   }


   // remove each file of each session
   SessionListIter sessionIter = removedSessions.begin();
   for( ; sessionIter != removedSessions.end(); sessionIter++) // CLIENT SESSIONS LOOP
   { // walk over all client sessions: cleanup each session
      Session* session = *sessionIter;
      NumNodeID sessionID = session->getSessionID();
      SessionLocalFileStore* sessionFiles = session->getLocalFiles();

      SessionLocalFileList removedSessionFiles;
      StringList referencedSessionFiles;

      sessionFiles->removeAllSessions(&removedSessionFiles, &referencedSessionFiles);

      // print sessionFiles results (upfront)
      if(removedSessionFiles.size() || referencedSessionFiles.size() )
      {
         std::ostringstream logMsgStream;
         logMsgStream << sessionID << ": Removing " << removedSessionFiles.size() <<
            " file sessions. " << "(" << referencedSessionFiles.size() << " are referenced)";
         LogContext(logContext).log(LogTopic_STATESYNC, Log_NOTICE, logMsgStream.str() );
      }

      SessionLocalFileListIter fileIter = removedSessionFiles.begin();

      for( ; fileIter != removedSessionFiles.end(); fileIter++) // SESSION FILES LOOP
      { // walk over all files: close
         SessionLocalFile* sessionFile = *fileIter;
         int fd = sessionFile->getFD();
         std::string fileHandleID = sessionFile->getFileHandleID();

         // close file descriptor
         if(fd != -1)
         {
            int closeRes = MsgHelperIO::close(fd);
            if(closeRes)
            { // close error
               LogContext(logContext).logErr("Unable to close local file. "
                  "FD: " + StringTk::intToStr(fd) + ". " +
                  "SysErr: " + System::getErrString() );
            }
            else
            { // success
               LogContext(logContext).log(Log_DEBUG, std::string("Local file closed.") +
                  " FD: " + StringTk::intToStr(fd) + ";" +
                  " HandleID: " + fileHandleID);
            }
         }


         delete(sessionFile);

      } // end of session files loop


      delete(session);

   } // end of client sessions loop
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadExceededQuotaList(QuotaDataType idType, QuotaLimitType exType,
   UIntList* outIDList, FhgfsOpsErr& error)
{
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();

   bool retVal = false;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   RequestExceededQuotaMsg msg(idType, exType);

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   RequestExceededQuotaRespMsg* respMsgCast = NULL;

   // connect & communicate
   commRes = MessagingTk::requestResponse(*mgmtNode, &msg, NETMSGTYPE_RequestExceededQuotaResp,
      &respBuf, &respMsg);
   if(!commRes)
      goto err_exit;

   // handle result
   respMsgCast = (RequestExceededQuotaRespMsg*)respMsg;

   respMsgCast->getExceededQuotaIDs()->swap(*outIDList);
   error = respMsgCast->getError();

   retVal = true;

   // cleanup
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

err_exit:
   return retVal;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAllExceededQuotaLists()
{
   const char* logContext = "Exceeded quota sync";

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   ExceededQuotaStore* exceededQuotaStore = app->getExceededQuotaStore();

   bool retVal = true;

   UIntList tmpExceededUIDsSize;
   UIntList tmpExceededGIDsSize;
   UIntList tmpExceededUIDsInode;
   UIntList tmpExceededGIDsInode;

   FhgfsOpsErr error;

   if(downloadExceededQuotaList(QuotaDataType_USER, QuotaLimitType_SIZE, &tmpExceededUIDsSize,
      error) )
   {
      exceededQuotaStore->updateExceededQuota(&tmpExceededUIDsSize, QuotaDataType_USER,
         QuotaLimitType_SIZE);
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file size quota for users.");
      retVal = false;
   }

   // enable or disable quota enforcement
   if(error == FhgfsOpsErr_SUCCESS)
   {
      if(!cfg->getQuotaEnableEnforcement() )
      {
         LogContext(logContext).log(Log_DEBUG,
            "Quota enforcement is enabled on the management daemon, "
            "but not in the configuration of this storage server. "
            "The configuration from the management daemon overrides the local setting.");
      }
      else
      {
         LogContext(logContext).log(Log_DEBUG, "Quota enforcement enabled by management daemon.");
      }

      cfg->setQuotaEnableEnforcement(true);
   }
   else
   {
      if(cfg->getQuotaEnableEnforcement() )
      {
         LogContext(logContext).log(Log_DEBUG,
               "Quota enforcement is enabled in the configuration of this storage server, "
               "but not on the management daemon. "
               "The configuration from the management daemon overrides the local setting.");
      }
      else
      {
         LogContext(logContext).log(Log_DEBUG, "Quota enforcement disabled by management daemon.");
      }

      cfg->setQuotaEnableEnforcement(false);
      return true;
   }

   if(downloadExceededQuotaList(QuotaDataType_GROUP, QuotaLimitType_SIZE, &tmpExceededGIDsSize,
      error) )
   {
      exceededQuotaStore->updateExceededQuota(&tmpExceededGIDsSize, QuotaDataType_GROUP,
         QuotaLimitType_SIZE);
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file size quota for groups.");
      retVal = false;
   }

   if(downloadExceededQuotaList(QuotaDataType_USER, QuotaLimitType_INODE, &tmpExceededUIDsInode,
      error) )
   {
      exceededQuotaStore->updateExceededQuota(&tmpExceededUIDsInode, QuotaDataType_USER,
         QuotaLimitType_INODE);
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file number quota for users.");
      retVal = false;
   }

   if(downloadExceededQuotaList(QuotaDataType_USER, QuotaLimitType_INODE, &tmpExceededGIDsInode,
      error) )
   {
      exceededQuotaStore->updateExceededQuota(&tmpExceededGIDsInode, QuotaDataType_GROUP,
         QuotaLimitType_INODE);
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file number quota for groups.");
      retVal = false;
   }

   return retVal;
}

/**
 * Tell mgmtd to update its capacity pools.
 */
void InternodeSyncer::forceMgmtdPoolsRefresh()
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      log.log(Log_DEBUG, "Management node not defined.");
      return;
   }

   RefreshCapacityPoolsMsg msg;

   bool ackReceived = dgramLis->sendToNodeUDPwithAck(mgmtNode, &msg);

   if (!ackReceived)
      log.log(Log_DEBUG, "Management node did not accept pools refresh request.");
}
