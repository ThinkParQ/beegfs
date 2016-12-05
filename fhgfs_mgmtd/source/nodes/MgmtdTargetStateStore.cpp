#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/threading/RWLockGuard.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/ZipIterator.h>
#include <components/HeartbeatManager.h>
#include <program/Program.h>

#include "MgmtdTargetStateStore.h"

#define MGMTDTARGETSTATESTORE_TMPFILE_EXT ".tmp"

MgmtdTargetStateStore::MgmtdTargetStateStore(NodeType nodeType) : TargetStateStore(),
   resyncSetDirty(false),
   nodeType(nodeType)
{
}

/**
 * targetIDs and consistencyStates is a 1:1 mapping.
 * @param setOnline whether to set targets for which a report was received to online and reset their
 *                  lastChangedTime or not.
 */
void MgmtdTargetStateStore::setConsistencyStatesFromLists(const UInt16List& targetIDs,
   const UInt8List& consistencyStates, bool setOnline)
{
   HeartbeatManager* heartbeatManager = Program::getApp()->getHeartbeatMgr();

   bool statesChanged = false;

   SafeRWLock lock(&rwlock, SafeRWLock_WRITE); // L O C K

   for (ZipConstIterRange<UInt16List, UInt8List> iter(targetIDs, consistencyStates);
        !iter.empty(); ++iter)
   {
      const uint16_t targetID = *iter()->first;
      TargetStateInfo& targetState = statesMap[targetID];

      const TargetConsistencyState newConsistencyState =
         static_cast<TargetConsistencyState>(*iter()->second);

      const TargetReachabilityState newReachabilityState =
         setOnline ? TargetReachabilityState_ONLINE : targetState.reachabilityState;

      if (targetState.reachabilityState != newReachabilityState
            || targetState.consistencyState != newConsistencyState)
      {
         targetState.reachabilityState = newReachabilityState;
         targetState.consistencyState = newConsistencyState;

         resyncSetUpdate(newConsistencyState, targetID);

         statesChanged = true;
      }

      if (setOnline)
         targetState.lastChangedTime.setToNow();
      // Note: If setOnline is false, we let the timer run out so the target can be POFFLINEd.
   }

   lock.unlock(); // U N L O C K

   if (statesChanged)
      heartbeatManager->notifyAsyncRefreshTargetStates();
}

/**
 * Changes the consistency state for all targets in the targetID list from a consistent known state
 * to a new state, i.e. if for each target the current target state matches the oldState for the
 * target, the change is stored. If for at least one target the oldState does not match the state
 * currently stored in the statesMap, no changes are committed to the statesMap and
 * FhgfsOpsErr_AGAIN is returned.
 *
 * targetIDs, oldStates and newStates is a 1:1 Mapping.
 *
 * @param buddyGroups Pointer to MirrorBuddyGroupMapper. If defined, a switchover will be triggered
 *                    if a secondary comes ONLINE and is GOOD while the primary is OFFLINE. Ignored
 *                    if NULL.
 */
FhgfsOpsErr MgmtdTargetStateStore::changeConsistencyStatesFromLists(const UInt16List& targetIDs,
   const UInt8List& oldStates, const UInt8List& newStates, MirrorBuddyGroupMapper* buddyGroups)
{
   HeartbeatManager* heartbeatManager = Program::getApp()->getHeartbeatMgr();

   const char* logContext = "Change consistency states";

   FhgfsOpsErr res = FhgfsOpsErr_SUCCESS;

   typedef std::vector<std::pair<uint16_t, TargetConsistencyState> > StateUpdateVec;
   StateUpdateVec updatedStates;
   updatedStates.reserve(statesMap.size() );

   SafeRWLock lock(&rwlock, SafeRWLock_WRITE); // L O C K

   for (ZipConstIterRange<UInt16List, UInt8List, UInt8List> iter(targetIDs, oldStates, newStates);
        !iter.empty(); ++iter)
   {
      const TargetStateInfo& targetState = statesMap[*iter()->first];

      if (targetState.consistencyState == *iter()->second)
      {
         updatedStates.push_back(StateUpdateVec::value_type(*iter()->first,
            static_cast<TargetConsistencyState>(*iter()->third) ) );
      }
      else
      {
         res = FhgfsOpsErr_AGAIN;
         break;
      }
   }

   bool statesChanged = false;

   if (res == FhgfsOpsErr_SUCCESS)
   {
      for (StateUpdateVec::const_iterator updatesIter = updatedStates.begin();
           updatesIter != updatedStates.end(); ++updatesIter)
      {
         const uint16_t targetID = updatesIter->first;
         TargetStateInfo& targetState = statesMap[targetID];

         const TargetConsistencyState newConsistencyState = updatesIter->second;

         bool targetComingOnline = targetState.reachabilityState != TargetReachabilityState_ONLINE;

         if (targetState.consistencyState != newConsistencyState
            || targetState.reachabilityState != TargetReachabilityState_ONLINE)
         {
            targetState.consistencyState = newConsistencyState;

            // Set target to online if we successfully received a state report.
            targetState.reachabilityState = TargetReachabilityState_ONLINE;

            resyncSetUpdate(newConsistencyState, targetID);

            statesChanged = true;
         }

         targetState.lastChangedTime.setToNow();

         // If a secondary target comes back online and the primary is offline, switch buddies.
         if (buddyGroups &&
             targetComingOnline &&
             (newConsistencyState == TargetConsistencyState_GOOD) )
         {
            buddyGroups->rwlock.writeLock(); // L O C K buddyGroups

            uint16_t primaryTargetID;
            bool getStateRes;
            CombinedTargetState primaryTargetState;

            LOG_DEBUG(logContext, Log_DEBUG, nodeTypeStr(true) + " coming online, checking if "
               "switchover is necessary. " + nodeTypeStr(true) + " ID: "
               + StringTk::uintToStr(targetID) );

            bool isPrimary;
            uint16_t buddyGroupID = buddyGroups->getBuddyGroupIDUnlocked(targetID, &isPrimary);

            if (isPrimary || !buddyGroupID) // If target is primary or unmapped, do nothing.
               goto bgroups_unlock;

            primaryTargetID = buddyGroups->getPrimaryTargetIDUnlocked(buddyGroupID);

            getStateRes = getStateUnlocked(primaryTargetID, primaryTargetState);
            if (!getStateRes)
            {
               LogContext(logContext).log(Log_ERR, "Failed to get state for mirror "
                  + nodeTypeStr(false) + " ID " + StringTk::uintToStr(primaryTargetID) );

               goto bgroups_unlock;
            }
            if (primaryTargetState.reachabilityState == TargetReachabilityState_OFFLINE)
               buddyGroups->switchover(buddyGroupID);

         bgroups_unlock:
            buddyGroups->rwlock.unlock(); // U N L O C K buddyGroups
         }

         // Log if a target is coming back online.
         if (targetComingOnline)
            LogContext(logContext).log(Log_WARNING,
               nodeTypeStr(true) + " is coming online. ID: " + StringTk::uintToStr(targetID) );
      }
   }

   lock.unlock(); // U N L O C K

   if (statesChanged)
      heartbeatManager->notifyAsyncRefreshTargetStates();

   return res;
}

/**
 * Automatically set targets to (P)OFFLINE if there was no state report received in a while.
 * Note: This uses the lastChangedTime of the TargetStateInfo object. It does not update the
 * timestamp though, so that the timestamp is effectively the time of the last non-timeout state
 * change.
 *
 * @param pofflineTimeoutMS Interval in which a target state update must have occurred so the target
                          is not POFFLINEd.
 * @param offlineTimeoutMS Time after which a target is OFFLINEd.
 * @param buddyGroups if provided, a switch will be made in the buddy groups when necessary (may
 *    be NULL).
 * @returns whether any reachability state was changed.
 */
bool MgmtdTargetStateStore::autoOfflineTargets(const unsigned pofflineTimeoutMS,
   const unsigned offlineTimeoutMS, MirrorBuddyGroupMapper* buddyGroups)
{
   const char* logContext = "Auto-offline";
   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG,
         "Checking for offline nodes. NodeType: " + nodeTypeStr(true));

   bool retVal = false;
   UInt16List offlinedTargets;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K targetStates

   /* buddyGroups lock right from the start here is necessary to have state changes and
      corresponding member switch atomic for outside viewers */
   if(buddyGroups)
      buddyGroups->rwlock.writeLock(); // L O C K buddyGroups

   Time nowTime;

   for (TargetStateInfoMapIter stateInfoIter = statesMap.begin(); stateInfoIter != statesMap.end();
        ++stateInfoIter)
   {
      uint16_t targetID = stateInfoIter->first;
      TargetStateInfo& targetStateInfo = stateInfoIter->second;
      const unsigned timeSinceLastUpdateMS = nowTime.elapsedSinceMS(
         &targetStateInfo.lastChangedTime);

      if (timeSinceLastUpdateMS > offlineTimeoutMS)
      {
         if (targetStateInfo.reachabilityState != TargetReachabilityState_OFFLINE)
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
               "No state report received from " + nodeTypeStr(false) + " for " +
               StringTk::uintToStr(timeSinceLastUpdateMS / 1000) + " seconds. "
               "Setting " + nodeTypeStr(false) + " to offline. "
               + nodeTypeStr(true) + " ID: " + StringTk::uintToStr(targetID) );

            offlinedTargets.push_back(targetID);

            targetStateInfo.reachabilityState = TargetReachabilityState_OFFLINE;
            retVal = true;
         }
      }
      else
      if (timeSinceLastUpdateMS > pofflineTimeoutMS)
      {
         if (targetStateInfo.reachabilityState != TargetReachabilityState_POFFLINE)
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
               "No state report received from " + nodeTypeStr(false) + " for "
               + StringTk::uintToStr(timeSinceLastUpdateMS / 1000) + " seconds. "
               "Setting " + nodeTypeStr(false) + " to probably-offline. " +
               nodeTypeStr(true) + " ID: " + StringTk::uintToStr(targetID) );

            targetStateInfo.reachabilityState = TargetReachabilityState_POFFLINE;
            retVal = true;
         }
      }
   }

   // do switchover in buddy groups if necessary
   if (buddyGroups)
   {
      for (UInt16ListIter targetIDIter = offlinedTargets.begin();
           targetIDIter != offlinedTargets.end(); ++targetIDIter)
      {
         bool isPrimary;
         uint16_t buddyGroupID = buddyGroups->getBuddyGroupIDUnlocked(*targetIDIter, &isPrimary);

         if (!isPrimary) // Target is secondary or unmapped -> no switchover necessary/possble.
            continue;

         uint16_t secondaryTargetID = buddyGroups->getSecondaryTargetIDUnlocked(buddyGroupID);

         // Get secondary's state and make sure it's online and good.
         CombinedTargetState secondaryTargetState;
         bool getStateRes = getStateUnlocked(secondaryTargetID, secondaryTargetState);
         if (!getStateRes)
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
               "Tried to switch mirror group members, "
               "but refusing to switch because secondary " + nodeTypeStr(false) +
               " state is unknown. Primary " + nodeTypeStr(false) + "ID: "
               + StringTk::uintToStr(*targetIDIter) );
            continue;
         }

         if ( (secondaryTargetState.reachabilityState != TargetReachabilityState_ONLINE) ||
              (secondaryTargetState.consistencyState != TargetConsistencyState_GOOD) )
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
               "Tried to switch mirror group members, "
               "but refusing to switch due to secondary " + nodeTypeStr(false) + " state. "
               "Secondary " + nodeTypeStr(false) + " ID: " + StringTk::uintToStr(*targetIDIter) +
               "; secondary state: " + TargetStateStore::stateToStr(secondaryTargetState) );
            continue;
         }

         buddyGroups->switchover(buddyGroupID);
      }

      buddyGroups->rwlock.unlock(); // U N L O C K buddyGroups
   }

   safeLock.unlock(); // U N L O C K targetStates

   return retVal;
}

/**
 * If both targets of any mirror buddy group need a resync, sets the primary to GOOD.
 * @returns true if any target state was changed, false otherwise.
 */
bool MgmtdTargetStateStore::resolveDoubleResync()
{
   UInt16Vector goodTargetsList = findDoubleResync();
   // Now look at the list of targets collected in the iteration and set their consistency state
   // (on the storage servers and in the local state store) to GOOD.
   return setTargetsGood(goodTargetsList);
}

/**
 * If any target/node is a primary of a buddy group, and needs a resync, a switchover in that
 * buddy group will be triggered.
 * @returns true if any group was switched over.
 */
bool MgmtdTargetStateStore::resolvePrimaryResync()
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapper* mbgm;
   if (nodeType == NODETYPE_Meta)
      mbgm = app->getMetaBuddyGroupMapper();
   else
      mbgm = app->getStorageBuddyGroupMapper();

   UInt16Vector groupsToSwitch = findPrimaryResync();
   for (auto groupIDIt = groupsToSwitch.begin(); groupIDIt != groupsToSwitch.end(); ++groupIDIt)
   {
      RWLockGuard lock(mbgm->rwlock, SafeRWLock_WRITE);
      mbgm->switchover(*groupIDIt);
   }

   return false;
}


/**
 * Finds mirror groups where both targets / nodes are in needs-resync state.
 * @returns a list of the primary targets.
 */
UInt16Vector MgmtdTargetStateStore::findDoubleResync()
{
   const char* logContext = "Resolve double resync";
   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG, "Checking for double-resync.");

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* mirrorBuddyGroupMapper;
   if (nodeType == NODETYPE_Meta)
      mirrorBuddyGroupMapper = app->getMetaBuddyGroupMapper();
   else
      mirrorBuddyGroupMapper = app->getStorageBuddyGroupMapper();

   UInt16Vector goodTargetsList; // List of targets that should be set to GOOD.

   UInt16List buddyGroupIDs; // just a dummy so we can use getMappingsAsLists
   MirrorBuddyGroupList buddyGroups;

   mirrorBuddyGroupMapper->getMappingAsLists(buddyGroupIDs, buddyGroups);

   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for (MirrorBuddyGroupListCIter buddyGroupIter = buddyGroups.begin();
         buddyGroupIter != buddyGroups.end(); ++buddyGroupIter)
   {
      const MirrorBuddyGroup& mirrorBuddyGroup = *buddyGroupIter;
      const uint16_t primaryTargetID = mirrorBuddyGroup.firstTargetID;
      const uint16_t secondaryTargetID = mirrorBuddyGroup.secondTargetID;

      static const CombinedTargetState onlineNeedsResyncState(
            TargetReachabilityState_ONLINE, TargetConsistencyState_NEEDS_RESYNC);

      CombinedTargetState primaryState;
      const bool primaryStateRes = getStateUnlocked(primaryTargetID, primaryState);

      if (!primaryStateRes)
      {
         LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
            "Failed to get state for " + nodeTypeStr(false) + " "
            + StringTk::uintToStr(primaryTargetID));

         continue;
      }

      if (primaryState == onlineNeedsResyncState)
      {
         CombinedTargetState secondaryState;
         const bool secondaryStateRes = getState(secondaryTargetID, secondaryState);

         if (!secondaryStateRes)
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
               "Failed to get state for " + nodeTypeStr(false) + " "
               + StringTk::uintToStr(primaryTargetID) );

            continue;
         }

         if (secondaryState == onlineNeedsResyncState)
         {
            // Both targets are online and need a resync. This means both have possibly corrupt
            // data on them. We can't tell which is better, but we have to decide for one or the
            // other. So we take the current primary because it is probably more up to date.

            LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
               "Both " + nodeTypeStr(false) + "s of a mirror group need a resync. "
               "Setting primary " + nodeTypeStr(false) + " to state good; "
               "Primary " + nodeTypeStr(false) + " ID: " + StringTk::uintToStr(primaryTargetID) );

            goodTargetsList.push_back(primaryTargetID);
         }
      }
   }

   return goodTargetsList;
}

UInt16Vector MgmtdTargetStateStore::findPrimaryResync()
{
   const char* logContext = "Resolve primary resync";
   LogContext(logContext).log(LogTopic_STATESYNC, Log_DEBUG,
         "Checking for primary that needs resync.");

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* mbgm;
   if (nodeType == NODETYPE_Meta)
      mbgm = app->getMetaBuddyGroupMapper();
   else
      mbgm = app->getStorageBuddyGroupMapper();

   UInt16Vector groupsToSwitch;

   MirrorBuddyGroupMap groups;
   mbgm->getMirrorBuddyGroups(groups);

   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for (auto buddyGroupIter = groups.cbegin(); buddyGroupIter != groups.end(); ++buddyGroupIter)
   {
      const uint16_t primaryTargetID = buddyGroupIter->second.firstTargetID;
      const uint16_t secondaryTargetID = buddyGroupIter->second.secondTargetID;
      const uint16_t mirrorGroupID = buddyGroupIter->first;

      CombinedTargetState primaryState;
      const bool primaryStateRes = getStateUnlocked(primaryTargetID, primaryState);
      if (!primaryStateRes)
      {
         LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
               "Failed to get state for " + nodeTypeStr(false) + " "
               + StringTk::uintToStr(primaryTargetID));

         continue;
      }

      if (primaryState.consistencyState == TargetConsistencyState_NEEDS_RESYNC)
      {
         // The primary is in needs resync state. This is not a valid state for a buddy group to be
         // in, so we need to switch over if the secondary is NOT bad or in needs resync state, too
         // (i.e. it is GOOD) - and we don't care whether it's online or offline.

         CombinedTargetState secondaryState;
         const bool secondaryStateRes = getStateUnlocked(secondaryTargetID, secondaryState);
         if (!secondaryStateRes)
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_ERR,
                  "Failed to get state for " + nodeTypeStr(false) + " "
                  + StringTk::uintToStr(secondaryTargetID));

            continue;
         }

         if (secondaryState.consistencyState == TargetConsistencyState_GOOD
             && secondaryState.reachabilityState == TargetReachabilityState_ONLINE)
         {
            LogContext(logContext).log(LogTopic_STATESYNC, Log_WARNING,
                  "The primary " + nodeTypeStr(false) + " of mirror group " +
                  StringTk::uintToStr(mirrorGroupID) + " needs a resync. "
                  "Switching primary/secondary.");

            groupsToSwitch.push_back(mirrorGroupID);
         }
      }
   }

   return groupsToSwitch;
}

/**
 * @returns true if ANY target / node state was changed.
 */
bool MgmtdTargetStateStore::setTargetsGood(const UInt16Vector& ids)
{
   App* app = Program::getApp();
   NodeStore* nodes;

   if (nodeType == NODETYPE_Meta)
      nodes = app->getMetaNodes();
   else if (nodeType == NODETYPE_Storage)
      nodes = app->getStorageNodes();
   else
      return false;

   TargetMapper* targetMapper = app->getTargetMapper(); // only used for storage targets

   bool res = false;

   for (UInt16VectorConstIter idIter = ids.begin(); idIter != ids.end(); ++idIter)
   {
      const uint16_t id = *idIter;

      FhgfsOpsErr refNodeErr = FhgfsOpsErr_SUCCESS;

      NodeHandle node;
      if (nodeType == NODETYPE_Meta)
         node = nodes->referenceNode(NumNodeID(id));
      else
         node = nodes->referenceNodeByTargetID(id, targetMapper, &refNodeErr);

      if (refNodeErr != FhgfsOpsErr_SUCCESS)
      {
         // Only referencing the storage node can produce an error code.
         LOG_TOP(STATESYNC, ERR, "Error referencing storage node for target.", id);
         continue;
      }
      else if (!node)
      {
         LOG_TOP(STATESYNC, ERR, "Error referencing metadata node.", id);
         continue;
      }

      UInt16List idList(1, id);
      UInt8List stateList(1, TargetConsistencyState_GOOD);

      SetTargetConsistencyStatesMsg msg(nodeType, &idList, &stateList, false);

      // request/response
      char* respBuf = NULL;
      NetMessage* respMsg = NULL;
      bool commRes = MessagingTk::requestResponse(
            *node, &msg, NETMSGTYPE_SetTargetConsistencyStatesResp, &respBuf, &respMsg);
      if (!commRes)
      {
         LOG_TOP(STATESYNC, ERR, "Unable to set primar " + nodeTypeStr(false) +
               " to target state good.", id);
         continue;
      }

      SetTargetConsistencyStatesRespMsg* respMsgCast =
         static_cast<SetTargetConsistencyStatesRespMsg*>(respMsg);
      FhgfsOpsErr result = respMsgCast->getResult();

      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);

      if (result != FhgfsOpsErr_SUCCESS)
      {
         LOG_TOP(STATESYNC, ERR, "Error setting primary " + nodeTypeStr(false) +
               " to state good.", id);
      }
      else
      {
         LOG_TOP_DBG(STATESYNC, DEBUG, "Successfully set primary " + nodeTypeStr(false) +
               " to state good.", id);
         setConsistencyState(id, TargetConsistencyState_GOOD);
         res = true;
      }
   }

   return res;
}

/**
 * Saves the list of targets which need a resync to the targets_need_resync file.
 */
void MgmtdTargetStateStore::saveResyncSetToFile()
{
   const char* logContext = "Save to resync file";

   App* app = Program::getApp();

   Path mgmtdPath = *app->getMgmtdPath();
   Path targetsToResyncPath = mgmtdPath / resyncSetStorePath;

   std::string targetsToResyncTmpPath =
      targetsToResyncPath.str() + MGMTDTARGETSTATESTORE_TMPFILE_EXT;

   std::string targetsToResyncStr;

   {
      SafeRWLock safeLock(&resyncSetLock, SafeRWLock_READ); // L O C K

      for (TargetIDSetCIter targetIter = resyncSet.begin();
           targetIter != resyncSet.end(); ++targetIter)
         targetsToResyncStr += StringTk::uintToStr(*targetIter) + "\n";

      safeLock.unlock(); // U N L O C K
   }

   // Create tmp file.
   const int openFlags = O_CREAT | O_TRUNC | O_RDWR;
   int fd = open(targetsToResyncTmpPath.c_str(), openFlags, 0644);
   if (fd == -1)
   { // error
      LogContext(logContext).log(Log_ERR, "Could not open temporary file: "
         + targetsToResyncTmpPath + "; SysErr: " + System::getErrString() );

      return;
   }

   ssize_t writeRes = write(fd, targetsToResyncStr.c_str(), targetsToResyncStr.length() );
   if (writeRes != (ssize_t)targetsToResyncStr.length() )
   {
      LogContext(logContext).log(Log_ERR, "Writing to file " + targetsToResyncTmpPath +
         " failed; SysErr: " + System::getErrString() );

      close(fd);
      return;
   }

   fsync(fd);
   close(fd);

   // Rename tmp file to actual file name.
   int renameRes =
      rename(targetsToResyncTmpPath.c_str(), targetsToResyncPath.str().c_str() );
   if (renameRes == -1)
   {
      LogContext(logContext).log(Log_ERR, "Renaming file " + targetsToResyncPath.str()
         + " to " + targetsToResyncPath.str() + " failed; SysErr: "
         + System::getErrString() );
   }

   { // Clear dirty flag.
      SafeRWLock safeLock(&resyncSetLock, SafeRWLock_READ); // L O C K
      resyncSetDirty = false;
      safeLock.unlock(); // U N L O C K
   }
}

/**
 * Reads the list of targets which need a resync from the targets_need_resync file.
 * Note: We assume the set is empty here, because this method is only called at startup.
 */
bool MgmtdTargetStateStore::loadResyncSetFromFile() throw (InvalidConfigException)
{
   const char* logContext = "Read resync file";

   if (!resyncSetStorePath.length() )
      return false;

   App* app = Program::getApp();

   Path mgmtdPath = *app->getMgmtdPath();
   Path targetsToResyncPath = mgmtdPath / resyncSetStorePath;

   StringList targetsToResyncList;

   bool fileExists = StorageTk::pathExists(targetsToResyncPath.str() );
   if (fileExists)
      ICommonConfig::loadStringListFile(targetsToResyncPath.str().c_str(),
         targetsToResyncList);

   SafeRWLock safeLock(&resyncSetLock, SafeRWLock_WRITE); // L O C K

   for (StringListIter targetsIter = targetsToResyncList.begin();
        targetsIter != targetsToResyncList.end(); ++targetsIter)
   {
      uint16_t targetID = StringTk::strToUInt(*targetsIter);

      if (targetID != 0)
         resyncSet.insert(targetID);
      else
         LogContext(logContext).log(Log_ERR,
            "Failed to deserialize " + nodeTypeStr(false) + " ID " + *targetsIter);
   }

   resyncSetDirty = true;

   safeLock.unlock(); // U N L O C K

   SafeRWLock safeReadLock(&resyncSetLock, SafeRWLock_READ); // L O C K

   // Set targets in the list to NEEDS_RESYNC.
   for (TargetIDSetCIter targetIDIter = resyncSet.begin();
        targetIDIter != resyncSet.end(); ++targetIDIter)
      addOrUpdate(*targetIDIter, CombinedTargetState(TargetReachabilityState_POFFLINE,
         TargetConsistencyState_NEEDS_RESYNC) );

   safeReadLock.unlock(); // U N L O C K

   return true;
}
