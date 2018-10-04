#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/threading/RWLockGuard.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/TempFileTk.h>
#include <common/toolkit/ZipIterator.h>
#include <components/HeartbeatManager.h>
#include <program/Program.h>

#include "MgmtdTargetStateStore.h"

#define MGMTDTARGETSTATESTORE_TMPFILE_EXT ".tmp"

MgmtdTargetStateStore::MgmtdTargetStateStore(NodeType nodeType) : TargetStateStore(nodeType)
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

   {
      RWLockGuard lock(rwlock, SafeRWLock_WRITE);

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

            LOG_DBG(STATES, DEBUG, "Setting new state.", targetID,
                  ("New state", stateToStr(targetState)),
                  nodeType, ("Called from", Backtrace<3>()));
            statesChanged = true;
         }

         if (setOnline)
            targetState.lastChangedTime.setToNow();
         // Note: If setOnline is false, we let the timer run out so the target can be POFFLINEd.
      }
   }

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
   const UInt8List& oldStates, const UInt8List& newStates, MirrorBuddyGroupMapperEx* buddyGroups)
{
   HeartbeatManager* heartbeatManager = Program::getApp()->getHeartbeatMgr();

   const char* logContext = "Change consistency states";

   FhgfsOpsErr res = FhgfsOpsErr_SUCCESS;

   typedef std::vector<std::pair<uint16_t, TargetConsistencyState> > StateUpdateVec;
   StateUpdateVec updatedStates;
   updatedStates.reserve(statesMap.size() );

   bool statesChanged = false;

   {
      RWLockGuard lock(rwlock, SafeRWLock_WRITE);

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

      if (res == FhgfsOpsErr_SUCCESS)
      {
         for (StateUpdateVec::const_iterator updatesIter = updatedStates.begin();
               updatesIter != updatedStates.end(); ++updatesIter)
         {
            const uint16_t targetID = updatesIter->first;
            TargetStateInfo& targetState = statesMap[targetID];

            const TargetConsistencyState newConsistencyState = updatesIter->second;

            bool targetComingOnline =
                  targetState.reachabilityState != TargetReachabilityState_ONLINE;

            if (targetState.consistencyState != newConsistencyState
                  || targetState.reachabilityState != TargetReachabilityState_ONLINE)
            {
               targetState.consistencyState = newConsistencyState;

               // Set target to online if we successfully received a state report.
               targetState.reachabilityState = TargetReachabilityState_ONLINE;

               statesChanged = true;

               LOG_DBG(STATES, DEBUG, "Changing state.", targetID,
                     ("New state", stateToStr(targetState)), nodeType,
                     ("Called from", Backtrace<3>()));
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
                     nodeTypeStr(true) + " is coming online. ID: " + StringTk::uintToStr(targetID));
         }
      }
   }

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
   const unsigned offlineTimeoutMS, MirrorBuddyGroupMapperEx* buddyGroups)
{
   const char* logContext = "Auto-offline";
   LogContext(logContext).log(LogTopic_STATES, Log_DEBUG,
         "Checking for offline nodes. NodeType: " + nodeTypeStr(true));

   bool retVal = false;
   UInt16List offlinedTargets;

   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

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
            LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
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
            LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
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
            LogContext(logContext).log(LogTopic_STATES, Log_ERR,
               "Tried to switch mirror group members, "
               "but refusing to switch because secondary " + nodeTypeStr(false) +
               " state is unknown. Primary " + nodeTypeStr(false) + "ID: "
               + StringTk::uintToStr(*targetIDIter) );
            continue;
         }

         if ( (secondaryTargetState.reachabilityState != TargetReachabilityState_ONLINE) ||
              (secondaryTargetState.consistencyState != TargetConsistencyState_GOOD) )
         {
            LogContext(logContext).log(LogTopic_STATES, Log_ERR,
               "Tried to switch mirror group members, "
               "but refusing to switch due to secondary " + nodeTypeStr(false) + " state. "
               "Current secondary " + nodeTypeStr(false)
               + " ID: " + StringTk::uintToStr(secondaryTargetID)
               + "; secondary state: " + TargetStateStore::stateToStr(secondaryTargetState) );
            continue;
         }

         buddyGroups->switchover(buddyGroupID);
      }

      buddyGroups->rwlock.unlock(); // U N L O C K buddyGroups
   }

   return retVal;
}

/**
 * If any target/node is a primary of a buddy group, and needs a resync, a switchover in that
 * buddy group will be triggered.
 * @returns true if any group was switched over.
 */
bool MgmtdTargetStateStore::resolvePrimaryResync()
{
   App* app = Program::getApp();
   MirrorBuddyGroupMapperEx* mbgm;
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

   return !groupsToSwitch.empty();
}

UInt16Vector MgmtdTargetStateStore::findPrimaryResync()
{
   const char* logContext = "Resolve primary resync";
   LOG_CTX(STATES, DEBUG, logContext, "Checking for primary that needs resync.", nodeType);

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
         LogContext(logContext).log(LogTopic_STATES, Log_ERR,
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
            LogContext(logContext).log(LogTopic_STATES, Log_ERR,
                  "Failed to get state for " + nodeTypeStr(false) + " "
                  + StringTk::uintToStr(secondaryTargetID));

            continue;
         }

         if (secondaryState.consistencyState == TargetConsistencyState_GOOD
             && secondaryState.reachabilityState == TargetReachabilityState_ONLINE)
         {
            LogContext(logContext).log(LogTopic_STATES, Log_WARNING,
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
 * Saves all the target states to a file.
 */
void MgmtdTargetStateStore::saveStatesToFile()
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   boost::scoped_array<char> buf;
   ssize_t serLen = serializeIntoNewBuffer(statesMap, buf);

   FhgfsOpsErr storeRes = FhgfsOpsErr_INVAL;
   if (serLen != -1)
   {
      Path targetStatesPath = *Program::getApp()->getMgmtdPath() / targetStateStorePath;
      storeRes = TempFileTk::storeTmpAndMove(targetStatesPath.str(),
            std::vector<char>(buf.get(), buf.get() + serLen));
   }

   if (storeRes != FhgfsOpsErr_SUCCESS)
      LOG(STATES, ERR, "Could not save target states.", nodeType);
}

bool MgmtdTargetStateStore::loadStatesFromFile()
{
   if (!targetStateStorePath.length())
      throw InvalidConfigException("State store path not set.");

   Path targetStatesPath = *Program::getApp()->getMgmtdPath() / targetStateStorePath;

   auto readRes = StorageTk::readFile(targetStatesPath.str(), 4 * 1024 * 1024);

   if (readRes.first != FhgfsOpsErr_SUCCESS)
   {
      LOG(STATES, ERR, "Could not read states.", nodeType,
            ("Error", readRes.first));
      return false;
   }

   if (readRes.second.empty()) // file was empty
   {
      LOG(STATES, WARNING, "Found empty target states file. "
          "Proceeding without last known target states.", nodeType);
      return true;
   }

   Deserializer des(&readRes.second[0], readRes.second.size());

   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   des % statesMap;

   if (!des.good())
   {
      LOG(STATES, ERR, "Could not deserialze states.", nodeType);
      return false;
   }

   LOG(STATES, NOTICE, "Loaded storage target states.", nodeType);

   // Set all targets / nodes to POFFLINE to prevent switchover before we have report from anyone.
   setAllStatesUnlocked(TargetReachabilityState_POFFLINE);

   return true;
}

/**
 * Reads the list of targets which need a resync from the targets_need_resync file.
 * Note: We assume the set is empty here, because this method is only called at startup.
 */
bool MgmtdTargetStateStore::loadResyncSetFromFile()
{
   const char* logContext = "Read resync file";

   if (!resyncSetStorePath.length() )
      return false;

   App* app = Program::getApp();

   Path mgmtdPath = *app->getMgmtdPath();
   Path targetsToResyncPath = mgmtdPath / resyncSetStorePath;

   StringList targetsToResyncList;

   bool fileExists = StorageTk::pathExists(targetsToResyncPath.str() );
   if (!fileExists)
      return false;

   ICommonConfig::loadStringListFile(targetsToResyncPath.str().c_str(),
         targetsToResyncList);

   TargetIDSet resyncSet;

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

   // Set targets in the list to NEEDS_RESYNC.
   for (TargetIDSetCIter targetIDIter = resyncSet.begin();
        targetIDIter != resyncSet.end(); ++targetIDIter)
   {
      addOrUpdate(*targetIDIter, CombinedTargetState(TargetReachabilityState_POFFLINE,
         TargetConsistencyState_NEEDS_RESYNC) );
      LOG_DBG(STATES, DEBUG, "Setting target/node to needs-resync.",
            ("ID", *targetIDIter), nodeType);
   }

   tempResyncSet.swap(resyncSet);

   return true;
}

/*
 * add or update target.
 *
 * if target already existed it gets updated;
 *
 * @return true if the target was either completely new or existed before but
 *         state changed.
 */
bool MgmtdTargetStateStore::addOrUpdate(uint16_t targetID, CombinedTargetState state)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   TargetStateInfoMapIter iter = statesMap.find(targetID);

   if (iter == statesMap.end() )
   {
      statesMap[targetID] = state;
      return true;
   }
   else if (iter->second != state)
   {
      iter->second = state; // Note: Also updates the lastChangedTime.
      return true;
   }

   return false;
}


