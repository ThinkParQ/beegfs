#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/ChangeTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/ChangeTargetConsistencyStatesRespMsg.h>
#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>
#include <common/net/message/storage/SetStorageTargetInfoMsg.h>
#include <common/net/message/storage/SetStorageTargetInfoRespMsg.h>
#include <common/net/message/storage/quota/RequestExceededQuotaMsg.h>
#include <common/net/message/storage/quota/RequestExceededQuotaRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/SessionTk.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/ZipIterator.h>
#include <common/nodes/NodeStore.h>
#include <common/nodes/TargetCapacityPools.h>
#include <app/App.h>
#include <app/config/Config.h>
#include <net/msghelpers/MsgHelperClose.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>
#include "InternodeSyncer.h"

#include <boost/lexical_cast.hpp>


InternodeSyncer::InternodeSyncer(TargetConsistencyState initialConsistencyState)
   : PThread("XNodeSync"),
     log("XNodeSync"),
     forcePoolsUpdate(true),
     forceTargetStatesUpdate(true),
     forcePublishCapacities(true),
     forceStoragePoolsUpdate(true),
     offlineWait(Program::getApp()->getConfig() ),
     nodeConsistencyState(initialConsistencyState),
     buddyResyncInProgress(false)
{
   MirrorBuddyGroupMapper* mbg = Program::getApp()->getMetaBuddyGroupMapper();
   MirrorBuddyState buddyState = mbg->getBuddyState(Program::getApp()->getLocalNodeNumID().val() );

   if ((buddyState == BuddyState_PRIMARY)
         && (nodeConsistencyState == TargetConsistencyState_NEEDS_RESYNC))
      offlineWait.startTimer();
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
   const App* app = Program::getApp();
   const Config* cfg = app->getConfig();

   const int sleepIntervalMS = 3*1000; // 3sec

   // If (undocumented) sysUpdateTargetStatesSecs is set in config, use that value, otherwise
   // default to 1/6 sysTargetOfflineTimeoutSecs.
   const unsigned updateTargetStatesMS =
      (cfg->getSysUpdateTargetStatesSecs() != 0)
      ? cfg->getSysUpdateTargetStatesSecs() * 1000
      : cfg->getSysTargetOfflineTimeoutSecs() * 166;

   const unsigned updateCapacityPoolsMS = 4 * updateTargetStatesMS;

   const unsigned metaCacheSweepNormalMS = 5*1000; // 5sec
   const unsigned metaCacheSweepStressedMS = 2*1000; // 2sec
   const unsigned idleDisconnectIntervalMS = 70*60*1000; /* 70 minutes (must be less than half the
      streamlis idle disconnect interval to avoid cases where streamlis disconnects first) */
   const unsigned updateIDTimeMS = 60 * 1000; // 1 min
   const unsigned downloadNodesIntervalMS = 300000; // 5 min
   const unsigned updateStoragePoolsMS = downloadNodesIntervalMS;
   const unsigned checkNetworkIntervalMS = 60*1000; // 1 minute

   Time lastCapacityUpdateT;
   Time lastMetaCacheSweepT;
   Time lastIdleDisconnectT;
   Time lastTimeIDSet;
   Time lastTargetStatesUpdateT;
   Time lastDownloadNodesT;
   Time lastStoragePoolsUpdateT;
   Time lastCapacityPublishedT;
   Time lastCheckNetworkT;
   bool doRegisterLocalNode = false;

   unsigned currentCacheSweepMS = metaCacheSweepNormalMS; // (adapted inside the loop below)


   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      const bool capacityPoolsUpdateForced = forcePoolsUpdate.exchange(false);
      const bool doCapacityPoolsUpdate = capacityPoolsUpdateForced
            || (lastCapacityUpdateT.elapsedMS() > updateCapacityPoolsMS);
      const bool doTargetStatesUpdate = forceTargetStatesUpdate.exchange(false)
            || (lastTargetStatesUpdateT.elapsedMS() > updateTargetStatesMS);
      const bool doPublishCapacities = forcePublishCapacities.exchange(false)
            || (lastCapacityPublishedT.elapsedMS() > updateTargetStatesMS);
      const bool doStoragePoolsUpdate = forceStoragePoolsUpdate.exchange(false)
            || (lastStoragePoolsUpdateT.elapsedMS() > updateStoragePoolsMS);
      const bool doCheckNetwork = forceCheckNetwork.exchange(false)
            || (lastCheckNetworkT.elapsedMS() > checkNetworkIntervalMS);

      if (doCheckNetwork)
      {
         if (checkNetwork())
            doRegisterLocalNode = true;
         lastCheckNetworkT.setToNow();
      }

      if (doRegisterLocalNode)
         doRegisterLocalNode = !registerNode(app->getDatagramListener());

      // download & sync nodes
      if (lastDownloadNodesT.elapsedMS() > downloadNodesIntervalMS)
      {
         downloadAndSyncNodes();
         downloadAndSyncTargetMappings();

         lastDownloadNodesT.setToNow();
      }

      if (doStoragePoolsUpdate)
      {
         downloadAndSyncStoragePools();

         lastStoragePoolsUpdateT.setToNow();
      }

      if(doCapacityPoolsUpdate)
      {
         updateMetaCapacityPools();
         updateMetaBuddyCapacityPools();

         if ( (capacityPoolsUpdateForced) && (!doStoragePoolsUpdate) )
         {  // capacity pools changed, but storage pools were not downloaded (i.e. no update on
            // storage capactity pools was made)
            updateStorageCapacityPools();
            updateTargetBuddyCapacityPools();
         }

         lastCapacityUpdateT.setToNow();
      }

      if(lastMetaCacheSweepT.elapsedMS() > currentCacheSweepMS)
      {
         bool flushTriggered = app->getMetaStore()->cacheSweepAsync();
         currentCacheSweepMS = (flushTriggered ? metaCacheSweepStressedMS : metaCacheSweepNormalMS);

         lastMetaCacheSweepT.setToNow();
      }

      if(lastIdleDisconnectT.elapsedMS() > idleDisconnectIntervalMS)
      {
         dropIdleConns();
         lastIdleDisconnectT.setToNow();
      }

      if(lastTimeIDSet.elapsedMS() > updateIDTimeMS)
      {
         StorageTk::resetIDCounterToNow();
         lastTimeIDSet.setToNow();
      }

      if(doTargetStatesUpdate)
      {
         if (this->offlineWait.hasTimeout() )
         {
            // if we're waiting to be offlined, set our local state to needs-resync and don't report
            // anything to the mgmtd
            setNodeConsistencyState(TargetConsistencyState_NEEDS_RESYNC);
         }
         else
         {
            TargetConsistencyState newConsistencyState;
            if (updateMetaStatesAndBuddyGroups(newConsistencyState, true))
               setNodeConsistencyState(newConsistencyState);
            downloadAndSyncTargetStatesAndBuddyGroups();
         }

         lastTargetStatesUpdateT.setToNow();
      }

      if (doPublishCapacities)
      {
         publishNodeCapacity();
         lastCapacityPublishedT.setToNow();
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

bool InternodeSyncer::updateMetaCapacityPools()
{
   NodeCapacityPools* pools = Program::getApp()->getMetaCapacityPools();

   auto downloadRes = downloadCapacityPools(CapacityPoolQuery_META);

   if(!downloadRes.first)
      return false;

   // poolsMap does only contain one element with INVALID_POOL_ID in this case
   const auto& capacityPoolLists = downloadRes.second[StoragePoolStore::INVALID_POOL_ID];

   pools->syncPoolsFromLists(capacityPoolLists);
   return true;
}

bool InternodeSyncer::updateMetaBuddyCapacityPools()
{
   NodeCapacityPools* pools = Program::getApp()->getMetaBuddyCapacityPools();

   auto downloadRes = downloadCapacityPools(CapacityPoolQuery_METABUDDIES);

   if(!downloadRes.first)
      return false;

   // poolsMap does only contain one element with INVALID_POOL_ID in this case
   const auto& capacityPoolLists = downloadRes.second[StoragePoolStore::INVALID_POOL_ID];

   pools->syncPoolsFromLists(capacityPoolLists);
   return true;
}

bool InternodeSyncer::updateStorageCapacityPools()
{
   const App* app = Program::getApp();
   const Config* cfg = app->getConfig();
   const TargetMapper* targetMapper = app->getTargetMapper();

   const auto downloadRes = downloadCapacityPools(CapacityPoolQuery_STORAGE);

   if(!downloadRes.first)
      return false;

   TargetMap targetMap;

   if(cfg->getSysTargetAttachmentMap() )
      targetMap = *cfg->getSysTargetAttachmentMap(); // user-provided custom assignment map
   else
      targetMap = targetMapper->getMapping();

   const GetNodeCapacityPoolsRespMsg::PoolsMap& poolsMap = downloadRes.second;

   bool failed = false;
   for (auto iter = poolsMap.begin(); iter != poolsMap.end(); iter++)
   {
      StoragePoolPtr storagePool = app->getStoragePoolStore()->getPool(iter->first);
      if (!storagePool)
      {
         LOG(CAPACITY, ERR, "Received capacity pools for unknown storage pool.",
             ("storagePoolId", iter->first));
         
         failed = true;
         continue;
      }

      storagePool->getTargetCapacityPools()->syncPoolsFromLists(iter->second, targetMap);
   }

   return !failed;
}

bool InternodeSyncer::updateTargetBuddyCapacityPools()
{
   const App* app = Program::getApp();

   auto downloadRes = downloadCapacityPools(CapacityPoolQuery_STORAGEBUDDIES);

   if(!downloadRes.first)
      return false;

   const GetNodeCapacityPoolsRespMsg::PoolsMap& poolsMap = downloadRes.second;

   bool failed = false;
   for (auto iter = poolsMap.begin(); iter != poolsMap.end(); iter++)
   {
      StoragePoolPtr storagePool = app->getStoragePoolStore()->getPool(iter->first);
      if (!storagePool)
      {
         LOG(CAPACITY, ERR, "Received capacity pools for unknown storage pool.",
             ("storagePoolId", iter->first));
         
         failed = true;
         continue;
      }

      storagePool->getBuddyCapacityPools()->syncPoolsFromLists(iter->second);
   }

   return !failed;
}

/**
 * @return a pair, with first being false on error/true on success and second being the downloaded
 *         map of capacity pools sorted by storage pool
 */
std::pair<bool, GetNodeCapacityPoolsRespMsg::PoolsMap> InternodeSyncer::downloadCapacityPools(
   CapacityPoolQueryType poolType)
{
   LOG(STATES, DEBUG, "Downloading capacity pools.",
       ("Pool type", GetNodeCapacityPoolsMsg::queryTypeToStr(poolType)));

   NodeStore* mgmtNodes = Program::getApp()->getMgmtNodes();

   auto node = mgmtNodes->referenceFirstNode();
   if(!node)
      return std::make_pair(false, GetNodeCapacityPoolsRespMsg::PoolsMap());

   GetNodeCapacityPoolsMsg msg(poolType);
   RequestResponseArgs rrArgs(node.get(), &msg, NETMSGTYPE_GetNodeCapacityPoolsResp);
   GetNodeCapacityPoolsRespMsg* respMsgCast;

#ifndef BEEGFS_DEBUG
   rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                    | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   // connect & communicate
   bool commRes = MessagingTk::requestResponse(&rrArgs);
   if(!commRes)
      return std::make_pair(false, GetNodeCapacityPoolsRespMsg::PoolsMap());

   // handle result
   respMsgCast = (GetNodeCapacityPoolsRespMsg*)rrArgs.outRespMsg.get();

   GetNodeCapacityPoolsRespMsg::PoolsMap poolsMap = respMsgCast->getPoolsMap();

   return std::make_pair(true, poolsMap);
}

/**
 * @return true if an ack was received for the heartbeat, false otherwise
 */
bool InternodeSyncer::registerNode(AbstractDatagramListener* dgramLis)
{
   const char* logContext = "Register node";
   static bool registrationFailureLogged = false; // to avoid log spamming

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   Config* cfg = app->getConfig();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   Node& localNode = Program::getApp()->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NumNodeID rootNodeID = app->getMetaRoot().getID();
   bool rootIsBuddyMirrored = app->getMetaRoot().getIsMirrored();
   NicAddressList nicList(localNode.getNicList());

   HeartbeatMsg msg(localNode.getID(), localNodeNumID, NODETYPE_Meta, &nicList);
   msg.setRootNumID(rootNodeID);
   msg.setRootIsBuddyMirrored(rootIsBuddyMirrored);
   msg.setPorts(cfg->getConnMetaPortUDP(), cfg->getConnMetaPortTCP() );

   bool nodeRegistered = dgramLis->sendToNodeUDPwithAck(mgmtNode, &msg);

   if(nodeRegistered)
      LogContext(logContext).log(Log_WARNING, "Node registration successful.");
   else
   if(!registrationFailureLogged)
   {
      LogContext(logContext).log(Log_CRITICAL,
         "Node registration not successful. Management node offline? Will keep on trying...");
      registrationFailureLogged = true;
   }

   return nodeRegistered;
}

/**
 * Download and sync metadata server target states and mirror buddy groups.
 *
 * @param outConsistencyState The new node consistency state.
 */
bool InternodeSyncer::updateMetaStatesAndBuddyGroups(TargetConsistencyState& outConsistencyState,
   bool publish)
{
   LOG(STATES, DEBUG, "Starting state update.");

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   TargetStateStore* metaStateStore = app->getMetaStateStore();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getMetaBuddyGroupMapper();

   static bool downloadFailedLogged = false; // to avoid log spamming
   static bool publishFailedLogged = false;

   NumNodeID localNodeID = app->getLocalNodeNumID();

   auto node = mgmtNodes->referenceFirstNode();
   if(!node)
   {
      LOG(STATES, ERR, "Management node not defined.");
      return false;
   }

   unsigned numRetries = 10; // If publishing states fails 10 times, give up (-> POFFLINE).

   // Note: Publishing fails if between downloadStatesAndBuddyGroups and
   // publishLocalTargetStateChanges, a state on the mgmtd is changed (e.g. because the primary
   // sets NEEDS_RESYNC for the secondary). In that case, we will retry.

   LOG(STATES, DEBUG, "Beginning target state update...");
   bool publishSuccess = false;

   while (!publishSuccess && (numRetries--) )
   {
      MirrorBuddyGroupMap buddyGroups;
      TargetStateMap states;

      bool downloadRes = NodesTk::downloadStatesAndBuddyGroups(*node, NODETYPE_Meta, buddyGroups,
         states, true);

      if (!downloadRes)
      {
         if (!downloadFailedLogged)
         {
            LOG(STATES, WARNING,
               "Downloading target states from management node failed. "
               "Setting all target states to probably-offline.");
            downloadFailedLogged = true;
         }

         metaStateStore->setAllStates(TargetReachabilityState_POFFLINE);

         break;
      }

      downloadFailedLogged = false;

      // Sync buddy groups here, because decideResync depends on it.
      metaStateStore->syncStatesAndGroups(buddyGroupMapper, states,
         std::move(buddyGroups), localNodeID);

      CombinedTargetState newStateFromMgmtd;
      // Find local state which was sent by mgmtd
      for (const auto& state : states)
      {
         if (state.first == localNodeID.val())
         {
            newStateFromMgmtd = CombinedTargetState(state.second.reachabilityState,
               state.second.consistencyState);
         }
      }

      TargetConsistencyState localChangedState = decideResync(newStateFromMgmtd);
      outConsistencyState = localChangedState;

      if (!publish)
      {
         metaStateStore->setState(localNodeID.val(),
            CombinedTargetState(TargetReachabilityState_ONLINE, localChangedState) );

         return true;
      }


      // Note: In this case "old" means "before we changed it locally".
      TargetConsistencyState oldState = newStateFromMgmtd.consistencyState;

      publishSuccess = publishNodeStateChange(oldState, localChangedState);

      if (publishSuccess)
      {
         metaStateStore->setState(localNodeID.val(),
            CombinedTargetState(TargetReachabilityState_ONLINE, localChangedState) );

         BuddyCommTk::checkBuddyNeedsResync();
      }
   }

   if (!publishSuccess)
   {
      if (!publishFailedLogged)
      {
         LOG(STATES, WARNING, "Pushing local state to management node failed.");
         publishFailedLogged = true;
      }
   }
   else
      publishFailedLogged = false;

   return true;
}

/**
 * Synchronize local client sessions with registered mgmtd clients to release orphaned sessions.
 *
 * @param clientsList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param allowRemoteComm usually true; setting this to false is only useful when called during
 * app shutdown to avoid communication; if false, unlocking of user locks, closing of storage server
 * files and disposal of unlinked files won't be performed
 */
void InternodeSyncer::syncClients(const std::vector<NodeHandle>& clientsList, bool allowRemoteComm)
{
   const char* logContext = "Sync clients";
   App* app = Program::getApp();
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   SessionStore* sessions = app->getSessions();
   SessionStore* mirroredSessions = app->getMirroredSessions();

   SessionList removedSessions;
   NumNodeIDList unremovableSessions;

   sessions->syncSessions(clientsList, removedSessions, unremovableSessions);
   mirroredSessions->syncSessions(clientsList, removedSessions, unremovableSessions);

   // print client session removal results (upfront)
   if(!removedSessions.empty() || !unremovableSessions.empty())
   {
      std::ostringstream logMsgStream;
      logMsgStream << "Removing " << removedSessions.size() << " client sessions.";

      if(unremovableSessions.empty() )
         LogContext(logContext).log(Log_DEBUG, logMsgStream.str() ); // no unremovable sessions
      else
      { // unremovable sessions found => log warning
         logMsgStream << " (" << unremovableSessions.size() << " are unremovable)";
         LogContext(logContext).log(Log_WARNING, logMsgStream.str() );
      }
   }


   // walk over all removed sessions (to cleanup the contained files)

   SessionListIter sessionIter = removedSessions.begin();
   for( ; sessionIter != removedSessions.end(); sessionIter++)
   { // walk over all client sessions: cleanup each session
      Session* session = *sessionIter;
      NumNodeID sessionID = session->getSessionID();
      SessionFileStore* sessionFiles = session->getFiles();

      SessionFileList removedSessionFiles;
      UIntList referencedSessionFiles;

      sessionFiles->removeAllSessions(&removedSessionFiles, &referencedSessionFiles);

      /* note: referencedSessionFiles should always be empty, because otherwise the reference holder
         would also hold a reference to the client session (and we woudn't be here if the client
         session had any references) */


      // print session files results (upfront)

      if (!removedSessionFiles.empty() || !referencedSessionFiles.empty())
      {
         std::ostringstream logMsgStream;
         logMsgStream << "Removing " << removedSessionFiles.size() << " file sessions. ("
            << referencedSessionFiles.size() << " are unremovable). clientNumID: " << sessionID;
         if (referencedSessionFiles.empty())
            LogContext(logContext).log(Log_SPAM, logMsgStream.str() );
         else
            LogContext(logContext).log(Log_NOTICE, logMsgStream.str() );
      }


      // walk over all files in the current session (to clean them up)

      SessionFileListIter fileIter = removedSessionFiles.begin();

      for( ; fileIter != removedSessionFiles.end(); fileIter++)
      { // walk over all files: unlock user locks, close meta, close local, dispose unlinked
         SessionFile* sessionFile = *fileIter;
         unsigned ownerFD = sessionFile->getSessionID();
         unsigned accessFlags = sessionFile->getAccessFlags();
         unsigned numHardlinks;
         unsigned numInodeRefs;

         MetaFileHandle inode = sessionFile->releaseInode();
         std::string fileID = inode->getEntryID();

         std::string fileHandleID = SessionTk::generateFileHandleID(ownerFD, fileID);

         // save nodeIDs for later
         StripePattern* pattern = inode->getStripePattern();
         int maxUsedNodesIndex = pattern->getStripeTargetIDs()->size() - 1;

         // unlock all user locks
         auto appendGranted = inode->flockAppendCancelByClientID(sessionID);
         auto flockGranted = inode->flockEntryCancelByClientID(sessionID);
         auto rangeGranted = inode->flockRangeCancelByClientID(sessionID);

         if(allowRemoteComm)
         {
            LockingNotifier::notifyWaitersEntryLock(LockEntryNotifyType_APPEND,
                  inode->getReferenceParentID(), inode->getEntryID(), inode->getIsBuddyMirrored(),
                  std::move(appendGranted));
            LockingNotifier::notifyWaitersEntryLock(LockEntryNotifyType_FLOCK,
                  inode->getReferenceParentID(), inode->getEntryID(), inode->getIsBuddyMirrored(),
                  std::move(flockGranted));
            LockingNotifier::notifyWaitersRangeLock(inode->getReferenceParentID(),
                  inode->getEntryID(), inode->getIsBuddyMirrored(), std::move(rangeGranted));
         }

         EntryInfo* entryInfo = sessionFile->getEntryInfo();

         FileIDLock lock(sessions->getEntryLockStore(), entryInfo->getEntryID(), true);

         if(allowRemoteComm)
            MsgHelperClose::closeChunkFile(sessionID, fileHandleID.c_str(),
               maxUsedNodesIndex, *inode, entryInfo, NETMSG_DEFAULT_USERID);

         LogContext(logContext).log(Log_NOTICE, "closing file. ParentID: " +
            entryInfo->getParentEntryID() + " FileName: " + entryInfo->getFileName() );

         metaStore->closeFile(entryInfo, std::move(inode), accessFlags, &numHardlinks,
               &numInodeRefs);

         if(allowRemoteComm && !numHardlinks && !numInodeRefs)
            MsgHelperClose::unlinkDisposableFile(fileID, NETMSG_DEFAULT_USERID,
                  entryInfo->getIsBuddyMirrored());

         delete sessionFile;
      } // end of files loop


      delete(session);

   } // end of client sessions loop
}

bool InternodeSyncer::downloadAndSyncNodes()
{
   LOG(STATES, DEBUG, "Starting node list sync.");

   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   Node& localNode = app->getLocalNode();
   NodeStore* metaNodes = app->getMetaNodes();
   NodeStore* storageNodes = app->getStorageNodes();

   // metadata nodes

   std::vector<NodeHandle> metaNodesList;
   NumNodeIDList addedMetaNodes;
   NumNodeIDList removedMetaNodes;
   NumNodeID rootNodeID;
   bool rootIsBuddyMirrored;

   if(NodesTk::downloadNodes(*mgmtNode, NODETYPE_Meta, metaNodesList, true, &rootNodeID,
      &rootIsBuddyMirrored) )
   {
      metaNodes->syncNodes(metaNodesList, &addedMetaNodes, &removedMetaNodes, &localNode);
      if (app->getMetaRoot().setIfDefault(rootNodeID, rootIsBuddyMirrored))
      {
         LOG(STATES, CRITICAL,
            std::string("Root node ID (from sync results): ") + rootNodeID.str());
         app->getRootDir()->setOwnerNodeID(rootNodeID);
      }

      printSyncResults(NODETYPE_Meta, &addedMetaNodes, &removedMetaNodes);
   }

   // storage nodes

   std::vector<NodeHandle> storageNodesList;
   NumNodeIDList addedStorageNodes;
   NumNodeIDList removedStorageNodes;

   if(NodesTk::downloadNodes(*mgmtNode, NODETYPE_Storage, storageNodesList, true) )
   {
      storageNodes->syncNodes(storageNodesList, &addedStorageNodes, &removedStorageNodes,
         &localNode);
      printSyncResults(NODETYPE_Storage, &addedStorageNodes, &removedStorageNodes);
   }

   return true;
}

void InternodeSyncer::downloadAndSyncClients(bool requeue)
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   NodeStoreClients* clientNodes = app->getClientNodes();
   TimerQueue* timerQ = app->getTimerQueue();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return;

   std::vector<NodeHandle> clientNodesList;
   NumNodeIDList addedClientNodes;
   NumNodeIDList removedClientNodes;

   if(NodesTk::downloadNodes(*mgmtNode, NODETYPE_Client, clientNodesList, true) )
   {
      clientNodes->syncNodes(clientNodesList, &addedClientNodes, &removedClientNodes);
      printSyncResults(NODETYPE_Client, &addedClientNodes, &removedClientNodes);

      syncClients(clientNodesList, true); // sync client sessions
   }

   if (requeue)
      timerQ->enqueue(std::chrono::minutes(5),
            [] { InternodeSyncer::downloadAndSyncClients(true); });
}

bool InternodeSyncer::downloadAndSyncTargetMappings()
{
   LOG(STATES, DEBUG, "Syncing target mappings.");

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStore* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   auto mappings = NodesTk::downloadTargetMappings(*mgmtNode, true);
   if (mappings.first)
      targetMapper->syncTargets(std::move(mappings.second));

   return true;
}

bool InternodeSyncer::downloadAndSyncStoragePools()
{
   LOG(STORAGEPOOLS, DEBUG, "Syncing storage pools.");

   const App* app = Program::getApp();
   const NodeStore* mgmtNodes = app->getMgmtNodes();
   StoragePoolStore* storagePoolStore = app->getStoragePoolStore();

   const auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   StoragePoolPtrVec storagePools;

   // note: storage pool download does include capacity pools
   const bool downloadStoragePoolsRes =
         NodesTk::downloadStoragePools(*mgmtNode, storagePools, true);
   if (!downloadStoragePoolsRes)
      return false;

   storagePoolStore->syncFromVector(storagePools);

   return true;
}

/**
 * Download and sync storage target states and mirror buddy groups.
 */
bool InternodeSyncer::downloadAndSyncTargetStatesAndBuddyGroups()
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   MirrorBuddyGroupMapper* buddyGroupMapper = app->getStorageBuddyGroupMapper();
   TargetStateStore* targetStates = app->getTargetStateStore();

   LOG(STATES, DEBUG, "Downloading target states and buddy groups");

   static bool downloadFailedLogged = false;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   TargetStateMap states;

   MirrorBuddyGroupMap buddyGroups;

   bool downloadRes = NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Storage,
      buddyGroups, states, true);

   if ( downloadRes )
   {
      targetStates->syncStatesAndGroups(buddyGroupMapper, states,
         std::move(buddyGroups), app->getLocalNode().getNumID());

      downloadFailedLogged = false;
   }
   else
   { // download failed, so we don't know actual status => carefully set all to poffline

      if(!downloadFailedLogged)
      {
         LOG(STATES, WARNING, "Download from management node failed. "
               "Setting all targets to probably-offline.");
         downloadFailedLogged = true;
      }

      targetStates->setAllStates(TargetReachabilityState_POFFLINE);
   }

   return true;
}

void InternodeSyncer::printSyncResults(NodeType nodeType, NumNodeIDList* addedNodes,
   NumNodeIDList* removedNodes)
{
   Logger* const logger = Logger::getLogger();
   const int logLevel = nodeType == NODETYPE_Client ? Log_DEBUG : Log_WARNING;

   if (!addedNodes->empty())
      logger->log(LogTopic_STATES, logLevel, __func__,
            std::string("Nodes added (sync results): ") +
            StringTk::uintToStr(addedNodes->size() ) +
            " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");

   if (!removedNodes->empty())
      logger->log(LogTopic_STATES, logLevel, __func__,
            std::string("Nodes removed (sync results): ") +
            StringTk::uintToStr(removedNodes->size() ) +
            " (Type: " + boost::lexical_cast<std::string>(nodeType) + ")");
}

/**
 * Decides new consistency state based on local state, buddy group membership and state fetched
 * from management node.
 *
 * @param newState New state from the management node.
 * @returns The new target consistency state.
 */
TargetConsistencyState InternodeSyncer::decideResync(const CombinedTargetState newState)
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();
   InternodeSyncer* internodeSyncer = app->getInternodeSyncer();
   NumNodeID localNodeID = app->getLocalNodeNumID();

   MirrorBuddyState buddyState = metaBuddyGroupMapper->getBuddyState(localNodeID.val());

   if (buddyState == BuddyState_UNMAPPED)
      return TargetConsistencyState_GOOD;

   // If the consistency state is BAD, it stays BAD until admin intervenes.
   if (internodeSyncer && // during early startup, INS is not constructed yet.
       internodeSyncer->getNodeConsistencyState() == TargetConsistencyState_BAD)
      return TargetConsistencyState_BAD;

   const bool isResyncing = newState.consistencyState == TargetConsistencyState_NEEDS_RESYNC;
   const bool isBad = newState.consistencyState == TargetConsistencyState_BAD;

   if (internodeSyncer &&
       internodeSyncer->getNodeConsistencyState() == TargetConsistencyState_NEEDS_RESYNC)
   {
      // If we're already (locally) maked as needs resync, this state can only be left when our
      // (primary) buddy tells us the resync is finished.
      return TargetConsistencyState_NEEDS_RESYNC;
   }
   else if (isResyncing || isBad)
   {
      return TargetConsistencyState_NEEDS_RESYNC;
   }
   else
   {
      // If mgmtd reports the target is (P)OFFLINE, then the meta server knows better and we set our
      // state to GOOD / ONLINE. Otherwise we accept the state reported by the mgmtd.
      if ( (newState.reachabilityState == TargetReachabilityState_OFFLINE)
        || (newState.reachabilityState == TargetReachabilityState_POFFLINE) )
         return TargetConsistencyState_GOOD;
      else
         return newState.consistencyState;
   }
}

bool InternodeSyncer::publishNodeStateChange(const TargetConsistencyState oldState,
   const TargetConsistencyState newState)
{
   const char* logContext = "Publish node state";
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   const NumNodeID localNodeID = app->getLocalNodeNumID();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      LogContext(logContext).logErr("Management node not defined.");
      return false;
   }

   bool res;

   UInt8List oldStateList(1, oldState);
   UInt8List newStateList(1, newState);
   UInt16List nodeIDList(1, localNodeID.val());

   ChangeTargetConsistencyStatesMsg msg(NODETYPE_Meta, &nodeIDList, &oldStateList, &newStateList);
   RequestResponseArgs rrArgs(mgmtNode.get(), &msg, NETMSGTYPE_ChangeTargetConsistencyStatesResp);

#ifndef BEEGFS_DEBUG
   rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                    | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   bool sendRes = MessagingTk::requestResponse(&rrArgs);

   static bool failureLogged = false;

   if (!sendRes)
   {
      if (!failureLogged)
         LogContext(logContext).log(Log_CRITICAL, "Pushing node state to management node failed.");

      res = false;
      failureLogged = true;
   }
   else
   {
      const auto respMsgCast =
            static_cast<const ChangeTargetConsistencyStatesRespMsg*>(rrArgs.outRespMsg.get());

      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(Log_CRITICAL, "Management node did not accept node state change.");
         res = false;
      }
      else
      {
         res = true;
      }

      failureLogged = false;
   }

   return res;
}

/**
 * Send local node free space / free inode info to management node.
 */
void InternodeSyncer::publishNodeCapacity()
{
   App* app = Program::getApp();
   NodeStore* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if (!mgmtNode)
   {
      log.logErr("Management node not defined.");
      return;
   }

   int64_t sizeTotal = 0;
   int64_t sizeFree = 0;
   int64_t inodesTotal = 0;
   int64_t inodesFree = 0;

   std::string metaPath = app->getMetaPath();
   getStatInfo(&sizeTotal, &sizeFree, &inodesTotal, &inodesFree);

   StorageTargetInfo targetInfo(app->getLocalNodeNumID().val(), metaPath, sizeTotal, sizeFree,
      inodesTotal, inodesFree, getNodeConsistencyState());
   // Note: As long as we don't have meta-HA, consistency state will always be GOOD.

   StorageTargetInfoList targetInfoList(1, targetInfo);

   SetStorageTargetInfoMsg msg(NODETYPE_Meta, &targetInfoList);
   RequestResponseArgs rrArgs(mgmtNode.get(), &msg, NETMSGTYPE_SetStorageTargetInfoResp);

#ifndef BEEGFS_DEBUG
   rrArgs.logFlags |= REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED
                    | REQUESTRESPONSEARGS_LOGFLAG_RETRY;
#endif // BEEGFS_DEBUG

   bool sendRes = MessagingTk::requestResponse(&rrArgs);

   static bool failureLogged = false;
   if (!sendRes)
   {
      if (!failureLogged)
         log.log(Log_CRITICAL, "Pushing node free space to management node failed.");

      failureLogged = true;
      return;
   }
   else
   {
      const auto respMsgCast = (const SetStorageTargetInfoRespMsg*)rrArgs.outRespMsg.get();
      failureLogged = false;

      if ( (FhgfsOpsErr)respMsgCast->getValue() != FhgfsOpsErr_SUCCESS)
      {
         log.log(Log_CRITICAL, "Management node did not accept free space info message.");
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

void InternodeSyncer::getStatInfo(int64_t* outSizeTotal, int64_t* outSizeFree,
   int64_t* outInodesTotal, int64_t* outInodesFree)
{
   const char* logContext = "GetStorageTargetInfoMsg (stat path)";

   std::string targetPathStr = Program::getApp()->getMetaPath();

   bool statSuccess = StorageTk::statStoragePath(targetPathStr, outSizeTotal, outSizeFree,
      outInodesTotal, outInodesFree);

   if(unlikely(!statSuccess) )
   { // error
      LogContext(logContext).logErr("Unable to statfs() storage path: " + targetPathStr +
         " (SysErr: " + System::getErrString() );

      *outSizeTotal = -1;
      *outSizeFree = -1;
      *outInodesTotal = -1;
      *outInodesFree = -1;
   }

   // read and use value from manual free space override file (if it exists)
   StorageTk::statStoragePathOverride(targetPathStr, outSizeFree, outInodesFree);
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



/**
 * @return false on error
 */
bool InternodeSyncer::downloadExceededQuotaList(StoragePoolId storagePoolId, QuotaDataType idType,
   QuotaLimitType exType, UIntList* outIDList, FhgfsOpsErr& error)
{
   App* app = Program::getApp();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();

   bool retVal = false;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   RequestExceededQuotaMsg msg(idType, exType, storagePoolId);

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

bool InternodeSyncer::downloadAllExceededQuotaLists(const StoragePoolPtrVec& storagePools)
{
   bool retVal = true;

   // note: this is fairly inefficient, but it is done only once on startup
   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      if (!downloadAllExceededQuotaLists(*iter))
         retVal = false;
   }

   return retVal;
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadAllExceededQuotaLists(const StoragePoolPtr storagePool)
{
   const char* logContext = "Exceeded quota sync";

   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   UInt16Set targets = storagePool->getTargets();

   bool retVal = true;

   UIntList tmpExceededUIDsSize;
   UIntList tmpExceededGIDsSize;
   UIntList tmpExceededUIDsInode;
   UIntList tmpExceededGIDsInode;

   FhgfsOpsErr error;

   if (downloadExceededQuotaList(storagePool->getId(), QuotaDataType_USER, QuotaLimitType_SIZE,
      &tmpExceededUIDsSize, error) )
   {
      // update exceeded store for every target in the pool
      for (auto iter = targets.begin(); iter != targets.end(); iter++)
      {
         uint16_t targetId = *iter;
         ExceededQuotaStorePtr exceededQuotaStore = app->getExceededQuotaStores()->get(targetId);
         if (!exceededQuotaStore)
         {
            LOG(STORAGEPOOLS, ERR, "Could not access exceeded quota store in file size quota for users.", targetId);
            retVal = false;
            break;
         }
         exceededQuotaStore->updateExceededQuota(&tmpExceededUIDsSize, QuotaDataType_USER,
            QuotaLimitType_SIZE);
      }

      // enable or disable quota enforcement
      if (error == FhgfsOpsErr_NOTSUPP)
      {
         if (cfg->getQuotaEnableEnforcement())
         {
            LogContext(logContext).log(Log_DEBUG,
               "Quota enforcement is enabled in the configuration of this metadata server, "
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
         if (!cfg->getQuotaEnableEnforcement())
         {
            LogContext(logContext).log(Log_DEBUG,
               "Quota enforcement is enabled on the management daemon, "
                  "but not in the configuration of this metadata server. "
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

   if (downloadExceededQuotaList(storagePool->getId(), QuotaDataType_GROUP, QuotaLimitType_SIZE,
      &tmpExceededGIDsSize, error))
   {
      for (auto iter = targets.begin(); iter != targets.end(); iter++)
      {
         uint16_t targetId = *iter;
         ExceededQuotaStorePtr exceededQuotaStore = app->getExceededQuotaStores()->get(targetId);
         if (!exceededQuotaStore)
         {
            LOG(STORAGEPOOLS, ERR, "Could not access exceeded quota store in file size quota for groups.", targetId);
            retVal = false;
            break;
         }
         exceededQuotaStore->updateExceededQuota(&tmpExceededGIDsSize, QuotaDataType_GROUP,
            QuotaLimitType_SIZE);
      }
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file size quota for groups.");
      retVal = false;
   }

   if (downloadExceededQuotaList(storagePool->getId(), QuotaDataType_USER, QuotaLimitType_INODE,
      &tmpExceededUIDsInode, error))
   {
      for (auto iter = targets.begin(); iter != targets.end(); iter++)
      {
         uint16_t targetId = *iter;
         ExceededQuotaStorePtr exceededQuotaStore = app->getExceededQuotaStores()->get(targetId);
         if (!exceededQuotaStore)
         {
            LOG(STORAGEPOOLS, ERR, "Could not access exceeded quota store in file number quota for users.", targetId);
            retVal = false;
            break;
         }
         exceededQuotaStore->updateExceededQuota(&tmpExceededUIDsInode, QuotaDataType_USER,
            QuotaLimitType_INODE);
      }
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file number quota for users.");
      retVal = false;
   }

   if (downloadExceededQuotaList(storagePool->getId(), QuotaDataType_USER, QuotaLimitType_INODE,
      &tmpExceededGIDsInode, error))
   {
      for (auto iter = targets.begin(); iter != targets.end(); iter++)
      {
         uint16_t targetId = *iter;
         ExceededQuotaStorePtr exceededQuotaStore = app->getExceededQuotaStores()->get(targetId);
         if (!exceededQuotaStore)
         {
            LOG(STORAGEPOOLS, ERR, "Could not access exceeded quota store in file number quota for groups.", targetId);
            retVal = false;
            break;
         }
         exceededQuotaStore->updateExceededQuota(&tmpExceededGIDsInode, QuotaDataType_GROUP,
            QuotaLimitType_INODE);
      }
   }
   else
   { // error
      LogContext(logContext).logErr("Unable to download exceeded file number quota for groups.");
      retVal = false;
   }

   return retVal;
}
