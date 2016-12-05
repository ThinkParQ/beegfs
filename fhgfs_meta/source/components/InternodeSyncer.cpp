#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
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
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>
#include <net/msghelpers/MsgHelperClose.h>
#include <app/App.h>
#include <app/config/Config.h>
#include "InternodeSyncer.h"


InternodeSyncer::InternodeSyncer(TargetConsistencyState initialConsistencyState)
   throw(ComponentInitException)
   : PThread("XNodeSync"),
     log("XNodeSync"),
     forcePoolsUpdate(true),
     forceTargetStatesUpdate(true),
     forcePublishCapacities(true),
     offlineWait(Program::getApp()->getConfig() ),
     nodeConsistencyState(initialConsistencyState),
     buddyResyncInProgress(false)
{
   MirrorBuddyGroupMapper* mbg = Program::getApp()->getMetaBuddyGroupMapper();
   MirrorBuddyState buddyState = mbg->getBuddyState(Program::getApp()->getLocalNodeNumID().val() );

   if ( (buddyState == BuddyState_PRIMARY)
     && (this->nodeConsistencyState == TargetConsistencyState_NEEDS_RESYNC) )
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
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   const int sleepIntervalMS = 3*1000; // 3sec

   // If (undocumented) sysUpdateTargetStatesSecs is set in config, use that value, otherwise
   // default to 1/3 sysTargetOfflineTimeoutSecs.
   const unsigned updateTargetStatesMS =
      (cfg->getSysUpdateTargetStatesSecs() != 0)
      ? cfg->getSysUpdateTargetStatesSecs() * 1000
      : cfg->getSysTargetOfflineTimeoutSecs() * 333;

   const unsigned updateCapacityPoolsMS = 2 * updateTargetStatesMS;

   const unsigned metaCacheSweepNormalMS = 5*1000; // 5sec
   const unsigned metaCacheSweepStressedMS = 2*1000; // 2sec
   const unsigned idleDisconnectIntervalMS = 70*60*1000; /* 70 minutes (must be less than half the
      streamlis idle disconnect interval to avoid cases where streamlis disconnects first) */
   const unsigned updateIDTimeMS = 60 * 1000; // 1 min
   const unsigned downloadNodesIntervalMS = 300000; // 5 min

   Time lastCapacityUpdateT;
   Time lastMetaCacheSweepT;
   Time lastIdleDisconnectT;
   Time lastTimeIDSet;
   Time lastTargetStatesUpdateT;
   Time lastDownloadNodesT;
   Time lastCapacityPublishedT;

   unsigned currentCacheSweepMS = metaCacheSweepNormalMS; // (adapted inside the loop below)


   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      bool doCapacityPoolsUpdate = getAndResetForcePoolsUpdate()
            || (lastCapacityUpdateT.elapsedMS() > updateCapacityPoolsMS);
      bool doTargetStatesUpdate = getAndResetForceTargetStatesUpdate()
            || (lastTargetStatesUpdateT.elapsedMS() > updateTargetStatesMS);
      bool doPublishCapacities = getAndResetForcePublishCapacities()
            || (lastCapacityPublishedT.elapsedMS() > updateTargetStatesMS);

      // download & sync nodes
      if(lastDownloadNodesT.elapsedMS() > downloadNodesIntervalMS)
      {
         downloadAndSyncNodes();
         downloadAndSyncTargetMappings();

         lastDownloadNodesT.setToNow();
      }

      if(doCapacityPoolsUpdate)
      {
         updateMetaCapacityPools();
         updateMetaBuddyCapacityPools();
         updateStorageCapacityPools();
         updateTargetBuddyCapacityPools();

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
            this->nodeConsistencyState = TargetConsistencyState_NEEDS_RESYNC;
         }
         else
         {
            updateMetaStatesAndBuddyGroups(this->nodeConsistencyState, true);
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

void InternodeSyncer::updateMetaCapacityPools()
{
   NodeCapacityPools* pools = Program::getApp()->getMetaCapacityPools();

   UInt16List listNormal;
   UInt16List listLow;
   UInt16List listEmergency;

   bool downloadRes = downloadCapacityPools(CapacityPoolQuery_META,
      &listNormal, &listLow, &listEmergency);

   if(!downloadRes)
      return;

   pools->syncPoolsFromLists(listNormal, listLow, listEmergency);
}

void InternodeSyncer::updateMetaBuddyCapacityPools()
{
   NodeCapacityPools* pools = Program::getApp()->getMetaBuddyCapacityPools();

   UInt16List listNormal;
   UInt16List listLow;
   UInt16List listEmergency;

   bool downloadRes = downloadCapacityPools(CapacityPoolQuery_METABUDDIES,
      &listNormal, &listLow, &listEmergency);

   if(!downloadRes)
      return;

   pools->syncPoolsFromLists(listNormal, listLow, listEmergency);
}

void InternodeSyncer::updateStorageCapacityPools()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   TargetCapacityPools* pools = app->getStorageCapacityPools();
   TargetMapper* targetMapper = app->getTargetMapper();

   UInt16List listNormal;
   UInt16List listLow;
   UInt16List listEmergency;

   bool downloadRes = downloadCapacityPools(CapacityPoolQuery_STORAGE,
      &listNormal, &listLow, &listEmergency);

   if(!downloadRes)
      return;

   TargetMap targetMap;

   if(cfg->getSysTargetAttachmentMap() )
      targetMap = *cfg->getSysTargetAttachmentMap(); // user-provided custom assignment map
   else
      targetMapper->getMapping(targetMap);

   pools->syncPoolsFromLists(listNormal, listLow, listEmergency, targetMap);
}

void InternodeSyncer::updateTargetBuddyCapacityPools()
{
   NodeCapacityPools* pools = Program::getApp()->getStorageBuddyCapacityPools();

   UInt16List listNormal;
   UInt16List listLow;
   UInt16List listEmergency;

   bool downloadRes = downloadCapacityPools(CapacityPoolQuery_STORAGEBUDDIES,
      &listNormal, &listLow, &listEmergency);

   if(!downloadRes)
      return;

   pools->syncPoolsFromLists(listNormal, listLow, listEmergency);
}

/**
 * @return false on error
 */
bool InternodeSyncer::downloadCapacityPools(CapacityPoolQueryType poolType,
   UInt16List* outListNormal, UInt16List* outListLow, UInt16List* outListEmergency)
{
   LOG_TOP(STATESYNC, DEBUG, "Downloading capacity pools.",
         as("Pool type", GetNodeCapacityPoolsMsg::queryTypeToStr(poolType)));

   NodeStore* mgmtNodes = Program::getApp()->getMgmtNodes();

   bool retVal = true;

   auto node = mgmtNodes->referenceFirstNode();
   if(!node)
      return false;

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
      return false;

   // handle result
   respMsgCast = (GetNodeCapacityPoolsRespMsg*)rrArgs.outRespMsg;

   respMsgCast->getNormal().swap(*outListNormal);
   respMsgCast->getLow().swap(*outListLow);
   respMsgCast->getEmergency().swap(*outListEmergency);

   return retVal;
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
   NodeStore* metaNodes = app->getMetaNodes();
   Config* cfg = app->getConfig();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   Node& localNode = Program::getApp()->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NumNodeID rootNodeID = metaNodes->getRootNodeNumID();
   bool rootIsBuddyMirrored = metaNodes->getRootIsBuddyMirrored();
   NicAddressList nicList(localNode.getNicList());
   const BitStore* nodeFeatureFlags = localNode.getNodeFeatures();

   HeartbeatMsg msg(localNode.getID(), localNodeNumID, NODETYPE_Meta, &nicList, nodeFeatureFlags);
   msg.setRootNumID(rootNodeID);
   msg.setRootIsBuddyMirrored(rootIsBuddyMirrored);
   msg.setPorts(cfg->getConnMetaPortUDP(), cfg->getConnMetaPortTCP() );
   msg.setFhgfsVersion(BEEGFS_VERSION_CODE);

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
   LOG_TOP(STATESYNC, DEBUG, "Starting state update.");

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
      LOG_TOP(STATESYNC, ERR, "Management node not defined.");
      return false;
   }

   UInt16List buddyGroupIDs;
   UInt16List primaryNodeIDs;
   UInt16List secondaryNodeIDs;

   UInt16List nodeIDs; // this should actually be targetIDs, but MDS doesn't have targets yet
   UInt8List reachabilityStates;
   UInt8List consistencyStates;

   unsigned numRetries = 10; // If publishing states fails 10 times, give up (-> POFFLINE).

   // Note: Publishing fails if between downloadStatesAndBuddyGroups and
   // publishLocalTargetStateChanges, a state on the mgmtd is changed (e.g. because the primary
   // sets NEEDS_RESYNC for the secondary). In that case, we will retry.

   LOG_TOP(STATESYNC, DEBUG, "Beginning target state update...");
   bool publishSuccess = false;

   while (!publishSuccess && (numRetries--) )
   {
      // In case we're already retrying, clear out leftover data from the lists.
      buddyGroupIDs.clear();
      primaryNodeIDs.clear();
      secondaryNodeIDs.clear();
      nodeIDs.clear();
      reachabilityStates.clear();
      consistencyStates.clear();


      bool downloadRes = NodesTk::downloadStatesAndBuddyGroups(*node, NODETYPE_Meta, &buddyGroupIDs,
         &primaryNodeIDs, &secondaryNodeIDs, &nodeIDs, &reachabilityStates, &consistencyStates,
         true);

      if (!downloadRes)
      {
         if (!downloadFailedLogged)
         {
            LOG_TOP(STATESYNC, WARNING,
               "Downloading target states from management node failed. "
               "Setting all target states to probably-offline.");
            downloadFailedLogged = true;
         }

         metaStateStore->setAllStates(TargetReachabilityState_POFFLINE);

         break;
      }

      downloadFailedLogged = false;

      UInt8List oldConsistencyStates = consistencyStates;

      // Sync buddy groups here, because decideResync depends on it.
      metaStateStore->syncStatesAndGroupsFromLists(buddyGroupMapper, nodeIDs, reachabilityStates,
         consistencyStates, buddyGroupIDs, primaryNodeIDs, secondaryNodeIDs, localNodeID);

      CombinedTargetState newStateFromMgmtd;
      // Find local state which was sent by mgmtd
      for (ZipIterRange<UInt16List, UInt8List, UInt8List>
           statesFromMgmtdIter(nodeIDs, reachabilityStates, consistencyStates);
           !statesFromMgmtdIter.empty(); ++statesFromMgmtdIter)
      {
         if (*(statesFromMgmtdIter()->first) == localNodeID.val())
         {
            newStateFromMgmtd = CombinedTargetState(
               TargetReachabilityState(*(statesFromMgmtdIter()->second) ),
               TargetConsistencyState(*(statesFromMgmtdIter()->third) ) );
         }
      }

      TargetConsistencyState localChangedState = decideResync(newStateFromMgmtd);

      if (!publish)
      {
         outConsistencyState = localChangedState;
         metaStateStore->setState(localNodeID.val(),
            CombinedTargetState(TargetReachabilityState_ONLINE, localChangedState) );

         return true;
      }


      // Note: In this case "old" means "before we changed it locally".
      TargetConsistencyState oldState = newStateFromMgmtd.consistencyState;

      publishSuccess = publishNodeStateChange(oldState, localChangedState);

      if (publishSuccess)
      {
         outConsistencyState = localChangedState;

         metaStateStore->setState(localNodeID.val(),
            CombinedTargetState(TargetReachabilityState_ONLINE, localChangedState) );

         BuddyCommTk::checkBuddyNeedsResync();
      }
   }

   if (!publishSuccess)
   {
      if (!publishFailedLogged)
      {
         LOG_TOP(STATESYNC, WARNING, "Pushing local state to management node failed.");
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
   SessionStore* mirroredSessions = app->getSessions();

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

      if(removedSessionFiles.size() || referencedSessionFiles.size() )
      {
         std::ostringstream logMsgStream;
         logMsgStream << sessionID << ": Removing " << removedSessionFiles.size() <<
            " file sessions. " << "(" << referencedSessionFiles.size() << " are unremovable)";
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

         if(allowRemoteComm)
            MsgHelperClose::closeChunkFile(sessionID, fileHandleID.c_str(),
               maxUsedNodesIndex, *inode, entryInfo, NETMSG_DEFAULT_USERID);

         LogContext(logContext).log(Log_NOTICE, std::string("closing file ") + "ParentID: " +
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
   LOG_TOP(STATESYNC, DEBUG, "Starting node list sync.");

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
      metaNodes->syncNodes(metaNodesList, &addedMetaNodes, &removedMetaNodes, true, &localNode);
      if(metaNodes->setRootNodeNumID(rootNodeID, false, rootIsBuddyMirrored) )
      {
         LOG_TOP(STATESYNC, CRITICAL,
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
      storageNodes->syncNodes(storageNodesList, &addedStorageNodes, &removedStorageNodes, true,
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
   Node& localNode = app->getLocalNode();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return;

   std::vector<NodeHandle> clientNodesList;
   NumNodeIDList addedClientNodes;
   NumNodeIDList removedClientNodes;

   if(NodesTk::downloadNodes(*mgmtNode, NODETYPE_Client, clientNodesList, true) )
   {
      clientNodes->syncNodes(clientNodesList, &addedClientNodes, &removedClientNodes, true,
         &localNode);
      printSyncResults(NODETYPE_Client, &addedClientNodes, &removedClientNodes);

      syncClients(clientNodesList, true); // sync client sessions
   }

   if (requeue)
      timerQ->enqueue(std::chrono::minutes(5),
            std::bind(InternodeSyncer::downloadAndSyncClients, true));
}

bool InternodeSyncer::downloadAndSyncTargetMappings()
{
   LOG_TOP(STATESYNC, DEBUG, "Syncing target mappings.");

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStore* mgmtNodes = app->getMgmtNodes();

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   UInt16List targetIDs;
   NumNodeIDList nodeIDs;

   bool downloadRes = NodesTk::downloadTargetMappings(*mgmtNode, &targetIDs, &nodeIDs, true);
   if(downloadRes)
      targetMapper->syncTargetsFromLists(targetIDs, nodeIDs);

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

   LOG_TOP(STATESYNC, DEBUG, "Downloading target states and buddy groups");

   static bool downloadFailedLogged = false;

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   UInt16List targetIDs;
   UInt8List reachabilityStates;
   UInt8List consistencyStates;

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;

   bool downloadRes = NodesTk::downloadStatesAndBuddyGroups(*mgmtNode, NODETYPE_Storage,
      &buddyGroupIDs, &primaryTargetIDs, &secondaryTargetIDs,
      &targetIDs, &reachabilityStates, &consistencyStates, true);

   if ( downloadRes )
   {
      targetStates->syncStatesAndGroupsFromLists(buddyGroupMapper, targetIDs, reachabilityStates,
         consistencyStates, buddyGroupIDs, primaryTargetIDs, secondaryTargetIDs,
         app->getLocalNode().getNumID());

      downloadFailedLogged = false;
   }
   else
   { // download failed, so we don't know actual status => carefully set all to poffline

      if(!downloadFailedLogged)
      {
         LOG_TOP(STATESYNC, WARNING, "Download from management node failed. "
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
   Logger* const logger = PThread::getCurrentThreadApp()->getLogger();
   const int logLevel = nodeType == NODETYPE_Client ? Log_DEBUG : Log_WARNING;

   if(addedNodes->size() )
      logger->log(LogTopic_STATESYNC, logLevel, __func__,
            std::string("Nodes added (sync results): ") +
            StringTk::uintToStr(addedNodes->size() ) +
            " (Type: " + Node::nodeTypeToStr(nodeType) + ")");

   if(removedNodes->size() )
      logger->log(LogTopic_STATESYNC, logLevel, __func__,
            std::string("Nodes removed (sync results): ") +
            StringTk::uintToStr(removedNodes->size() ) +
            " (Type: " + Node::nodeTypeToStr(nodeType) + ")");
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
       internodeSyncer->nodeConsistencyState == TargetConsistencyState_BAD)
      return TargetConsistencyState_BAD;

   const bool isResyncing = newState.consistencyState == TargetConsistencyState_NEEDS_RESYNC;
   const bool isBad = newState.consistencyState == TargetConsistencyState_BAD;

   if (internodeSyncer &&
       internodeSyncer->nodeConsistencyState == TargetConsistencyState_NEEDS_RESYNC)
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
      ChangeTargetConsistencyStatesRespMsg* respMsgCast =
         static_cast<ChangeTargetConsistencyStatesRespMsg*>(rrArgs.outRespMsg);

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
      inodesTotal, inodesFree, nodeConsistencyState);
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
      SetStorageTargetInfoRespMsg* respMsgCast = (SetStorageTargetInfoRespMsg*)rrArgs.outRespMsg;
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
               "but not in the configuration of this metadata server. "
               "The configuration from the management daemon overrides the local setting.");
      }
      else
      {
         LogContext(logContext).log(Log_DEBUG, "Quota enforcement enabled by management daemon.");
      }

      app->getConfig()->setQuotaEnableEnforcement(true);
   }
   else
   {
      if(cfg->getQuotaEnableEnforcement() )
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

      app->getConfig()->setQuotaEnableEnforcement(false);
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
