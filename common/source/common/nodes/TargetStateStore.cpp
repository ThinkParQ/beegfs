#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/toolkit/ZipIterator.h>
#include "TargetStateStore.h"

TargetStateStore::TargetStateStore(NodeType nodeType)
   : nodeType(nodeType)
{
}

/**
 * add only if target didn't exist yet, otherwise leave it unchanged.
 */
void TargetStateStore::addIfNotExists(uint16_t targetID, CombinedTargetState state)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   TargetStateInfoMapIter iter = statesMap.find(targetID);

   if (iter == statesMap.end())
   {
      statesMap[targetID] = state;
      LOG_DBG(STATES, DEBUG, "Adding new item to state store.", targetID,
            as("New state", stateToStr(state)), nodeType, as("Called from", Backtrace<3>()));
   }
   else
   {
      LOG_DBG(STATES, DEBUG,
            "Adding new item to state store failed because it already exists.", targetID,
            as("Called from", Backtrace<3>()));
   }
}

void TargetStateStore::removeTarget(uint16_t targetID)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   statesMap.erase(targetID);
}

/**
 * Atomically update target states and buddy groups together. This is important to avoid races
 * during a switch (e.g. where an outside viewer could see an offline primary).
 *
 * Of course, this implies that states and groups also have to be read atomically together via
 * getStatesAndGroupsAsLists() through GetStatesAndBuddyGroupsMsg.
 */
void TargetStateStore::syncStatesAndGroupsFromLists(MirrorBuddyGroupMapper* buddyGroups,
   const UInt16List& targetIDs, const UInt8List& reachabilityStates,
   const UInt8List& consistencyStates, const UInt16List& buddyGroupIDs,
   const UInt16List& primaryTargets, const UInt16List& secondaryTargets,
   const NumNodeID localNodeID)
{
   // create temporary states map first without lock, then swap with lock (to keep lock time short)

   TargetStateInfoMap statesMapNewTmp;
   for (ZipConstIterRange<UInt16List, UInt8List, UInt8List>
        iter(targetIDs, reachabilityStates, consistencyStates);
        !iter.empty(); ++iter)
   {
      TargetStateInfo stateInfo(
         static_cast<TargetReachabilityState>(*iter()->second),
         static_cast<TargetConsistencyState>(*iter()->third) );
      statesMapNewTmp[*iter()->first] = stateInfo;
      LOG_DBG(STATES, DEBUG, "Syncing state.", as("targetID", *iter()->first),
            as("New state", stateToStr(stateInfo)), as("Called from", Backtrace<3>()));
   }

   // pre-create a new buddy map and then swap elements (to keep lock time short)
   uint16_t localGroupID = 0;
   MirrorBuddyGroupMap newGroups;
   for ( ZipConstIterRange<UInt16List, UInt16List, UInt16List> iter(buddyGroupIDs, primaryTargets,
      secondaryTargets); !iter.empty(); ++iter )
   {
      newGroups[*iter()->first] = MirrorBuddyGroup(*iter()->second, *iter()->third);

      if (localNodeID == NumNodeID(*iter()->second)
         || localNodeID == NumNodeID(*iter()->third) )
         localGroupID = *iter()->first;
   }

   // now do the actual updates...

   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   RWLockGuard buddyGroupsLock(buddyGroups->rwlock, SafeRWLock_WRITE);

   statesMapNewTmp.swap(statesMap);

   if (localGroupID != 0)
      buddyGroups->setLocalGroupIDUnlocked(localGroupID);

   buddyGroups->mirrorBuddyGroups.swap(newGroups);

   buddyGroups->mappingsDirty = true;
}

/**
 * Note: If you're also updating mirror buddy groups, you probably rather want to call
 * syncStatesAndGroupsFromLists() instead of this.
 */
void TargetStateStore::syncStatesFromLists(const UInt16List& targetIDs,
   const UInt8List& reachabilityStates, const UInt8List& consistencyStates)
{
   // we fill a temporary map first without lock, then swap with lock (to keep lock time short)

   TargetStateInfoMap statesMapNewTmp;
   for (ZipConstIterRange<UInt16List, UInt8List, UInt8List>
        iter(targetIDs, reachabilityStates, consistencyStates);
        !iter.empty(); ++iter)
   {
      TargetStateInfo stateInfo(
         static_cast<TargetReachabilityState>(*iter()->second),
         static_cast<TargetConsistencyState>(*iter()->third) );
      statesMapNewTmp[*iter()->first] = stateInfo;
      LOG_DBG(STATES, DEBUG, "Assigning new state.", as("targetID", *iter()->first),
            as("New state", stateToStr(stateInfo)), as("Called from", Backtrace<3>()));
   }

   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   statesMapNewTmp.swap(statesMap);
}

/**
 * Atomically get target states and buddy groups together. This is important to avoid races
 * during a switch (e.g. where an outside viewer could see an offline primary).
 *
 * Of course, this implies that states and groups are also updated atomically together via
 * syncStatesAndGroupsFromLists() on the other side.
 */
void TargetStateStore::getStatesAndGroupsAsLists(MirrorBuddyGroupMapper* buddyGroups,
   UInt16List& outTargetIDs, UInt8List& outReachabilityStates, UInt8List& outConsistencyStates,
   UInt16List& outBuddyGroupIDs, UInt16List& outPrimaryTargets, UInt16List& outSecondaryTargets)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   RWLockGuard buddyGroupsLock(buddyGroups->rwlock, SafeRWLock_READ);

   getStatesAsListsUnlocked(outTargetIDs, outReachabilityStates, outConsistencyStates);
   buddyGroups->getMappingAsListsUnlocked(outBuddyGroupIDs, outPrimaryTargets, outSecondaryTargets);
}


/**
 * Note: If you're also getting mirror buddy groups, you probably rather want to call
 * getStatesAndGroupsAsLists() instead of this.
 */
void TargetStateStore::getStatesAsLists(UInt16List& outTargetIDs,
   UInt8List& outReachabilityStates, UInt8List& outConsistencyStates)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   getStatesAsListsUnlocked(outTargetIDs, outReachabilityStates, outConsistencyStates);
}

/**
 * Note: Caller must hold readlock.
 */
void TargetStateStore::getStatesAsListsUnlocked(UInt16List& outTargetIDs,
   UInt8List& outReachabilityStates, UInt8List& outConsistencyStates)
{
   for (TargetStateInfoMapIter iter = statesMap.begin(); iter != statesMap.end(); iter++)
   {
      outTargetIDs.push_back(iter->first);
      outReachabilityStates.push_back((uint8_t)(iter->second).reachabilityState);
      outConsistencyStates.push_back((uint8_t)(iter->second).consistencyState);
   }
}

const char* TargetStateStore::stateToStr(TargetReachabilityState state)
{
   switch(state)
   {
      case TargetReachabilityState_ONLINE:
         return "Online";

      case TargetReachabilityState_OFFLINE:
         return "Offline";

      case TargetReachabilityState_POFFLINE:
         return "Probably-offline";

      default:
         return "<invalid_value>";
   }
}

const char* TargetStateStore::stateToStr(TargetConsistencyState state)
{
   switch(state)
   {
      case TargetConsistencyState_GOOD:
         return "Good";

      case TargetConsistencyState_NEEDS_RESYNC:
         return "Needs-resync";

      case TargetConsistencyState_BAD:
         return "Bad";

      default:
         return "<invalid_value>";
   }
}

const std::string TargetStateStore::stateToStr(const CombinedTargetState& state)
{
   std::ostringstream resStr;
   resStr << stateToStr(state.reachabilityState) << " / " << stateToStr(state.consistencyState);
   return resStr.str();
}
