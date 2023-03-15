#include <app/config/Config.h>
#include <app/App.h>
#include <common/net/message/nodes/ChangeTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/ChangeTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/MapTargetsMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/GetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/GetTargetConsistencyStatesRespMsg.h>
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

#include <boost/lexical_cast.hpp>

InternodeSyncer::InternodeSyncer():
   PThread("XNodeSync"),
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
   const unsigned checkNetworkIntervalMS = 60*1000; // 1 minute
   const unsigned idleDisconnectIntervalMS = 70*60*1000; /* 70 minutes (must be less than half the
      streamlis idle disconnect interval to avoid cases where streamlis disconnects first) */
   const unsigned downloadNodesIntervalMS = 600000; // 10min
   const unsigned updateStoragePoolsMS = downloadNodesIntervalMS;

   // If (undocumented) sysUpdateTargetStatesSecs is set in config, use that value, otherwise
   // default to 1/6 sysTargetOfflineTimeoutSecs.
   const unsigned updateTargetStatesMS =
      (cfg->getSysUpdateTargetStatesSecs() != 0)
      ? cfg->getSysUpdateTargetStatesSecs() * 1000
      : cfg->getSysTargetOfflineTimeoutSecs() * 166;

   const unsigned updateCapacitiesMS = updateTargetStatesMS * 4;

   Time lastCacheSweepT;
   Time lastCheckNetworkT;
   Time lastIdleDisconnectT;
   Time lastDownloadNodesT;
   Time lastTargetStatesUpdateT;
   Time lastStoragePoolsUpdateT;
   Time lastCapacityUpdateT;
   bool doRegisterLocalNode = false;

   unsigned currentCacheSweepMS = sweepNormalMS; // (adapted inside the loop below)

   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      bool targetStatesUpdateForced = getAndResetForceTargetStatesUpdate();
      bool publishCapacitiesForced = getAndResetForcePublishCapacities();
      bool storagePoolsUpdateForced = getAndResetForceStoragePoolsUpdate();
      bool checkNetworkForced = getAndResetForceCheckNetwork();

      if(lastCacheSweepT.elapsedMS() > currentCacheSweepMS)
      {
         bool flushTriggered = app->getChunkDirStore()->cacheSweepAsync();
         currentCacheSweepMS = (flushTriggered ? sweepStressedMS : sweepNormalMS);

         lastCacheSweepT.setToNow();
      }

      if (checkNetworkForced ||
         (lastCheckNetworkT.elapsedMS() > checkNetworkIntervalMS))
      {
         if (checkNetwork())
            doRegisterLocalNode = true;
         lastCheckNetworkT.setToNow();
      }

      if (doRegisterLocalNode)
         doRegisterLocalNode = !registerNode(app->getDatagramListener());

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

      if (storagePoolsUpdateForced ||
         (lastStoragePoolsUpdateT.elapsedMS() > updateStoragePoolsMS))
      {
         downloadAndSyncStoragePools();

         lastStoragePoolsUpdateT.setToNow();
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
 * Inspect the available and allowed network interfaces for any changes.
 */
bool InternodeSyncer::checkNetwork()
{
   App* app = Program::getApp();
   NicAddressList newLocalNicList;
   bool res = false;

   app->findAllowedInterfaces(newLocalNicList);
   app->findAllowedRDMAInterfaces(newLocalNicList);
   if (!std::equal(newLocalNicList.begin(), newLocalNicList.end(), app->getLocalNicList().begin()))
   {
      log.log(Log_NOTICE, "checkNetwork: local interfaces have changed");
      app->updateLocalNicList(newLocalNicList);
      res = true;
   }

   return res;
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
unsigned InternodeSyncer::dropIdleConnsByStore(NodeStoreServers* nodes)
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

void InternodeSyncer::updateTargetStatesAndBuddyGroups()
{
   const char* logContext = "Update states and mirror groups";
   LogContext(logContext).log(LogTopic_STATES, Log_DEBUG,
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
      LogContext(logContext).log(LogTopic_STATES, Log_ERR, "Management node not defined.");
      return;
   }

   unsigned numRetries = 10; // If publishing states fails 10 times, give up (-> POFFLINE).

   // Note: Publishing states fails if between downloadStatesAndBuddyGroups and
   // publishLocalTargetStateChanges, a state on the mgmtd is changed (e.g. because the primary
   // sets NEEDS_RESYNC for the secondary). In that case, we will retry.

   LogContext(logContext).log(LogTopic_STATES, Log_DEBUG,
         "Beginning target state update...");
   bool publishSuccess = false;

   while (!publishSuccess && (numRetries--) )
   {
      MirrorBuddyGroupMap buddyGroups;
      TargetStateMap states;

      bool downloadRes = NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Storage,
         buddyGroups, states, true);

      if (!downloadRes)
      {
         if(!downloadFailedLogged)
         {
            LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
               "Downloading target states from management node failed. "
               "Setting all targets to probably-offline.");
            downloadFailedLogged = true;
         }

         targetStateStore->setAllStates(TargetReachabilityState_POFFLINE);

         break;
      }

      downloadFailedLogged = false;

      // before anything else is done, update the targetWasOffline flags in the resyncers. updating
      // them later opens a window of opportunity where the target state store says "offline", but
      // the resyncer has not noticed - which would erroneously not fail the resync.
      for (const auto& state : states)
      {
         if (state.second.reachabilityState == TargetReachabilityState_OFFLINE)
         {
            const auto job = app->getBuddyResyncer()->getResyncJob(state.first);
            if (job)
               job->setTargetOffline();
         }
      }

      // Sync buddy groups here, because decideResync depends on it.
      // This is not a problem because if pushing target states fails all targets will be
      // (p)offline anyway.
      targetStateStore->syncStatesAndGroups(mirrorBuddyGroupMapper, states, buddyGroups,
            app->getLocalNode().getNumID());

      TargetStateMap localTargetChangedStates;
      storageTargets->decideResync(states, localTargetChangedStates);

      publishSuccess = publishLocalTargetStateChanges(states, localTargetChangedStates);

      if(publishSuccess)
         storageTargets->checkBuddyNeedsResync();
   }

   if(!publishSuccess)
   {
      if(!publishFailedLogged)
      {
         log.log(LogTopic_STATES, Log_WARNING,
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

   log.log(LogTopic_STATES, Log_DEBUG, "Publishing target capacity infos.");

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      log.log(LogTopic_STATES, Log_ERR, "Management node not defined.");
      return;
   }

   StorageTargetInfoList targetInfoList;

   storageTargets->generateTargetInfoList(targetInfoList);

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
         log.log(LogTopic_STATES, Log_CRITICAL,
               "Pushing target free space info to management node failed.");

      failureLogged = true;
      return;
   }
   else
   {
      const auto respMsgCast =
            static_cast<const SetStorageTargetInfoRespMsg*>(rrArgs.outRespMsg.get());

      failureLogged = false;

      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
      {
         log.log(LogTopic_STATES, Log_CRITICAL,
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

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
         NETMSGTYPE_SetTargetConsistencyStatesResp);

   if (!respMsg)
      log.log(Log_CRITICAL, "Pushing target state to management node failed.");
   else
   {
      auto* respMsgCast = (SetTargetConsistencyStatesRespMsg*)respMsg.get();
      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
         log.log(Log_CRITICAL, "Management node did not accept target state.");
   }
}

/**
 * Gets a list of target states changes (old/new), and reports the local ones (targets which are
 * present in this storage server's storageTargetDataMap) to the mgmtd.
 */
bool InternodeSyncer::publishLocalTargetStateChanges(const TargetStateMap& oldStates,
      const TargetStateMap& changes)
{
   App* app = Program::getApp();
   StorageTargets* storageTargets = app->getStorageTargets();

   UInt16List localTargetIDs;
   UInt8List localOldStates;
   UInt8List localNewStates;

   for (const auto& state : oldStates)
   {
      const uint16_t targetID = state.first;
      auto* const target = storageTargets->getTarget(targetID);

      if (!target)
         continue;

      // Don't report targets which have an offline timeout at the moment.
      const auto waitRemaining = target->getOfflineTimeout();
      if (waitRemaining)
      {
         LOG(GENERAL, WARNING, "Target was a primary target and needs a resync. "
               "Waiting until it is marked offline on all clients.",
               targetID, ("remainingMS", waitRemaining->count()));
         continue;
      }

      localTargetIDs.push_back(state.first);
      localOldStates.push_back(state.second.consistencyState);

      const auto change = changes.find(state.first);

      if (change != changes.end())
         localNewStates.push_back(change->second.consistencyState);
      else
         localNewStates.push_back(state.second.consistencyState);
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

   HeartbeatMsg msg(localNode.getID(), localNodeNumID, NODETYPE_Storage, &nicList);
   msg.setPorts(cfg->getConnStoragePortUDP(), cfg->getConnStoragePortTCP() );

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
 * note: only called once at startup
 *
 * @return true if targets mapping successful.
 */
bool InternodeSyncer::registerTargetMappings()
{
   static std::map<uint16_t, bool> registrationFailureLogged; // one for eacht target; to avoid log
                                                              // spamming
   static bool commErrorLogged = false; // to avoid log spamming

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   bool registered = true;

   Node& localNode = app->getLocalNode();
   NumNodeID localNodeID = localNode.getNumID();
   StorageTargets* targets = Program::getApp()->getStorageTargets();
   std::map<uint16_t, StoragePoolId> targetPools;

   MapTargetsRespMsg* respMsgCast;

   // for each target, check if a storagePoolId file exists in the storage dir; if there is, try to
   // directly put the target in the specified pool when mapping at mgmtd
   // note: if file is not set readNumStoragePoolIDFile will return default pool
   for (const auto& mapping : targets->getTargets())
   {
      const auto& targetPath = mapping.second->getPath().str();

      targetPools.emplace(mapping.first,
            StorageTk::readNumStoragePoolIDFile(targetPath, STORAGETK_STORAGEPOOLID_FILENAME));
   }

   MapTargetsMsg msg(targetPools, localNodeID);

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg, NETMSGTYPE_MapTargetsResp);
   if (respMsg)
   {
      // handle result
      respMsgCast = (MapTargetsRespMsg*) respMsg.get();

      const auto& results = respMsgCast->getResults();

      for (const auto& mapping : targets->getTargets())
      {
         const auto targetID = mapping.first;
         const auto result = results.find(targetID);

         if (result == results.end())
         {
            registered = false;

            LOG(GENERAL, CRITICAL, "Mgmt ignored target registration attempt.", targetID);
            registrationFailureLogged[targetID] = true;
         }
         else if (result->second != FhgfsOpsErr_SUCCESS)
         {
            registered = false;

            if (!registrationFailureLogged[targetID])
            {
               LOG(GENERAL, CRITICAL, "Storage target registration rejected. Will keep on trying.",
                             targetID, ("error", result->second));
               registrationFailureLogged[targetID] = true;
            }
         }
         else
         {
            // registered successfully => remove STORAGETK_STORAGEPOOLID_FILENAME for this target,
            // because it is only relevant for first registration
            const auto& targetPath = mapping.second->getPath().str();
            std::string storagePoolIdFileName = targetPath + "/" + STORAGETK_STORAGEPOOLID_FILENAME;

            int unlinkRes = ::unlink(storagePoolIdFileName.c_str());
            int errorCode = errno;
            if ((unlinkRes != 0) && (errorCode != ENOENT))
            { // error; note: if file doesn't exist, that's not considered an error
               LOG(GENERAL, WARNING, "Unable to unlink storage pool ID file", targetID, errorCode);
            }
         }
      }
   }
   else if (!commErrorLogged)
   {
      LOG(GENERAL, CRITICAL, "Storage targets registration not successful. "
                             "Management node offline? Will keep on trying.");
      commErrorLogged = true;
   }

   if (registered)
   {
      LOG(GENERAL, WARNING, "Storage targets registration successful.");
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

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
      NETMSGTYPE_ChangeTargetConsistencyStatesResp);

   if (!respMsg)
   {
      log.log(Log_CRITICAL, "Pushing target state changes to management node failed.");
      res = false; // Retry.
   }
   else
   {
      auto* respMsgCast = (ChangeTargetConsistencyStatesRespMsg*)respMsg.get();

      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
      {
         log.log(Log_CRITICAL, "Management node did not accept target state changes.");
         res = false; // States were changed while we evaluated the state changed. Try again.
      }
      else
         res = true;
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

   LogContext(logContext).log(LogTopic_STATES, Log_DEBUG, "Requesting buddy target states.");

   // loop over all local targets
   for (const auto& mapping : storageTargets->getTargets())
   {
      uint16_t targetID = mapping.first;

      // check if target is part of a buddy group
      uint16_t buddyTargetID = buddyGroupMapper->getBuddyTargetID(targetID);
      if(!buddyTargetID)
         continue;

      // this target is part of a buddy group

      NumNodeID nodeID = targetMapper->getNodeID(buddyTargetID);
      if(!nodeID)
      { // mapping to node not found
         LogContext(logContext).log(LogTopic_STATES, Log_ERR,
            "Node-mapping for target ID " + StringTk::uintToStr(buddyTargetID) + " not found.");
         continue;
      }

      auto node = storageNodes->referenceNode(nodeID);
      if(!node)
      { // node not found
         LogContext(logContext).log(LogTopic_STATES, Log_ERR,
            "Unknown storage node. nodeID: " + nodeID.str() + "; targetID: "
               + StringTk::uintToStr(targetID));
         continue;
      }

      // get reachability state of buddy target ID
      CombinedTargetState currentState;
      targetStateStore->getState(buddyTargetID, currentState);

      if(currentState.reachabilityState == TargetReachabilityState_ONLINE)
      {
         // communicate
         UInt16Vector queryTargetIDs(1, buddyTargetID);
         GetTargetConsistencyStatesMsg msg(queryTargetIDs);

         const auto respMsg = MessagingTk::requestResponse(*node, msg,
               NETMSGTYPE_GetTargetConsistencyStatesResp);
         if (!respMsg)
         { // communication failed
            LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
               "Communication with buddy target failed. "
                  "nodeID: " + nodeID.str() + "; buddy targetID: "
                  + StringTk::uintToStr(buddyTargetID));

            continue;
         }

         // handle response
         auto respMsgCast = (GetTargetConsistencyStatesRespMsg*)respMsg.get();
         const auto& targetConsistencyStates = &respMsgCast->getStates();

         // get received target information
         // (note: we only requested a single target info, so the first one must be the
         // requested one)
         const TargetConsistencyState buddyTargetConsistencyState =
            targetConsistencyStates->empty() ? TargetConsistencyState_BAD :
               targetConsistencyStates->front();

         auto& target = *storageTargets->getTargets().at(targetID);

         // set last comm timestamp, but ignore it if we think buddy needs a resync
         const bool buddyNeedsResync = target.getBuddyNeedsResync();
         if((buddyTargetConsistencyState == TargetConsistencyState_GOOD) && !buddyNeedsResync)
            target.setLastBuddyComm(std::chrono::system_clock::now(), false);
      }
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
   LogContext(logContext).log(LogTopic_STATES, Log_DEBUG, "Called.");

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

      storageNodes->syncNodes(storageNodesList, &addedStorageNodes, &removedStorageNodes,
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

      metaNodes->syncNodes(metaNodesList, &addedMetaNodes, &removedMetaNodes);

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

   if (!addedNodes->empty())
      LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
         std::string("Nodes added: ") +
         StringTk::uintToStr(addedNodes->size() ) +
         " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");

   if (!removedNodes->empty())
      LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
         std::string("Nodes removed: ") +
         StringTk::uintToStr(removedNodes->size() ) +
         " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncTargetMappings()
{
   LogContext("Download target mappings").log(LogTopic_STATES, Log_DEBUG,
         "Syncing target mappings.");

   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   bool retVal = true;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   auto mappings = NodesTk::downloadTargetMappings(*mgmtNode, true);
   if (mappings.first)
      targetMapper->syncTargets(std::move(mappings.second));
   else
      retVal = false;

   return retVal;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAndSyncMirrorBuddyGroups()
{
   LogContext("Downlod mirror groups").log(LogTopic_STATES, Log_DEBUG,
         "Syncing mirror groups.");
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();

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
   }
   else
      retVal = false;

   return retVal;
}

bool InternodeSyncer::downloadAndSyncStoragePools()
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   StoragePoolStore* storagePoolStore = app->getStoragePoolStore();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   StoragePoolPtrVec storagePools;

   bool downloadPoolsRes = NodesTk::downloadStoragePools(*mgmtNode, storagePools, true);
   if(downloadPoolsRes)
      storagePoolStore->syncFromVector(storagePools);

   return true;
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
   LogContext(logContext).log(LogTopic_STATES, Log_DEBUG, "Client session sync started.");

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();

   auto removedSessions = sessions->syncSessions(clientsList);

   // print sessions removal results (upfront)
   if (!removedSessions.empty())
   {
      std::ostringstream logMsgStream;
      logMsgStream << "Removing " << removedSessions.size() << " client sessions. ";
      LogContext(logContext).log(LogTopic_STATES, Log_DEBUG, logMsgStream.str() );
   }


   // remove each file of each session
   auto sessionIter = removedSessions.begin();
   for( ; sessionIter != removedSessions.end(); sessionIter++) // CLIENT SESSIONS LOOP
   { // walk over all client sessions: cleanup each session
      auto& session = *sessionIter;
      NumNodeID sessionID = session->getSessionID();
      SessionLocalFileStore* sessionFiles = session->getLocalFiles();

      auto removed = sessionFiles->removeAllSessions();

      // print sessionFiles results (upfront)
      if (removed)
      {
         std::ostringstream logMsgStream;
         logMsgStream << sessionID << ": Removing " << removed << " file sessions.";
         LogContext(logContext).log(LogTopic_STATES, Log_NOTICE, logMsgStream.str() );
      }
   } // end of client sessions loop
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadExceededQuotaList(uint16_t targetId, QuotaDataType idType,
   QuotaLimitType exType, UIntList* outIDList, FhgfsOpsErr& error)
{
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();

   bool retVal = false;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   RequestExceededQuotaMsg msg(idType, exType, targetId);

   RequestExceededQuotaRespMsg* respMsgCast = NULL;

   const auto respMsg = MessagingTk::requestResponse(*mgmtNode, msg,
         NETMSGTYPE_RequestExceededQuotaResp);
   if (!respMsg)
      goto err_exit;

   // handle result
   respMsgCast = (RequestExceededQuotaRespMsg*)respMsg.get();

   respMsgCast->getExceededQuotaIDs()->swap(*outIDList);
   error = respMsgCast->getError();

   retVal = true;

err_exit:
   return retVal;
}

bool InternodeSyncer::downloadAllExceededQuotaLists(
      const std::map<uint16_t, std::unique_ptr<StorageTarget>>& targets)
{
   bool retVal = true;

   // note: this is fairly inefficient, but it is done only one on startup
   for (const auto& mapping : targets)
   {
      if (!downloadAllExceededQuotaLists(mapping.first))
         retVal = false;
   }

   return retVal;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAllExceededQuotaLists(uint16_t targetId)
{
   const char* logContext = "Exceeded quota sync";

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   ExceededQuotaStorePtr exceededQuotaStore = app->getExceededQuotaStores()->get(targetId);

   if (!exceededQuotaStore)
   {
      LOG(STORAGEPOOLS, ERR, "Could not access exceeded quota store.", targetId);
      return false;
   }

   bool retVal = true;

   UIntList tmpExceededUIDsSize;
   UIntList tmpExceededGIDsSize;
   UIntList tmpExceededUIDsInode;
   UIntList tmpExceededGIDsInode;

   FhgfsOpsErr error;

   if (downloadExceededQuotaList(targetId, QuotaDataType_USER, QuotaLimitType_SIZE,
      &tmpExceededUIDsSize, error) )
   {
      exceededQuotaStore->updateExceededQuota(&tmpExceededUIDsSize, QuotaDataType_USER,
         QuotaLimitType_SIZE);

      // enable or disable quota enforcement
      if(error == FhgfsOpsErr_NOTSUPP)
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
      else
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
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file size quota for users.");
      retVal = false;
   }

   if (downloadExceededQuotaList(targetId, QuotaDataType_GROUP, QuotaLimitType_SIZE,
      &tmpExceededGIDsSize, error))
   {
      exceededQuotaStore->updateExceededQuota(&tmpExceededGIDsSize, QuotaDataType_GROUP,
         QuotaLimitType_SIZE);
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file size quota for groups.");
      retVal = false;
   }

   if (downloadExceededQuotaList(targetId, QuotaDataType_USER, QuotaLimitType_INODE,
      &tmpExceededUIDsInode, error))
   {
      exceededQuotaStore->updateExceededQuota(&tmpExceededUIDsInode, QuotaDataType_USER,
         QuotaLimitType_INODE);
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file number quota for users.");
      retVal = false;
   }

   if (downloadExceededQuotaList(targetId, QuotaDataType_USER, QuotaLimitType_INODE,
      &tmpExceededGIDsInode, error))
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

