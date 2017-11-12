#include "TargetStateStore.h"


void TargetStateStore_init(TargetStateStore* this)
{
   RWLock_init(&this->rwlock);

   TargetStateInfoMap_init(&this->states);
}

TargetStateStore* TargetStateStore_construct(void)
{
   TargetStateStore* this = (TargetStateStore*)os_kmalloc(sizeof(*this) );

   if (likely(this) )
      TargetStateStore_init(this);

   return this;
}

void TargetStateStore_uninit(TargetStateStore* this)
{
   // map elements have to be destructed
   TargetStateInfoMapIter statesMapIter;

   statesMapIter = TargetStateInfoMap_begin(&this->states);
   for(/* iter init'ed above */;
       !TargetStateInfoMapIter_end(&statesMapIter);
       TargetStateInfoMapIter_next(&statesMapIter) )
   {
      TargetStateInfo* stateInfo = TargetStateInfoMapIter_value(&statesMapIter);
      TargetStateInfo_destruct(stateInfo);
   }

   TargetStateInfoMap_uninit(&this->states);

   RWLock_uninit(&this->rwlock);
}

void TargetStateStore_destruct(TargetStateStore* this)
{
   TargetStateStore_uninit(this);

   kfree(this);
}

/**
 * Atomically update target states and buddy groups together. This is important to avoid races
 * during a switch (e.g. where an outside viewer could see an offline primary).
 *
 * Of course, this implies that states and groups also have to be read atomically together via
 * getStatesAndGroupsAsLists() through GetStatesAndBuddyGroupsMsg.
 */
void TargetStateStore_syncStatesAndGroupsFromLists(TargetStateStore* this, Config* config,
   MirrorBuddyGroupMapper* buddyGroups, UInt16List* targetIDs, UInt8List* reachabilityStates,
   UInt8List* consistencyStates, UInt16List* buddyGroupIDs, UInt16List* primaryTargets,
   UInt16List* secondaryTargets)
{
   RWLock_writeLock(&this->rwlock); // L O C K targetStates
   RWLock_writeLock(&buddyGroups->rwlock); // L O C K buddyGroups

   __TargetStateStore_syncStatesFromListsUnlocked(this, targetIDs,
      reachabilityStates, consistencyStates);

   __MirrorBuddyGroupMapper_syncGroupsFromListsUnlocked(buddyGroups, config, buddyGroupIDs,
      primaryTargets, secondaryTargets);

   RWLock_writeUnlock(&buddyGroups->rwlock); // U N L O C K buddyGroups
   RWLock_writeUnlock(&this->rwlock); // U N L O C K targetStates
}


void TargetStateStore_syncStatesFromLists(TargetStateStore* this, UInt16List* targetIDs,
   UInt8List* reachabilityStates, UInt8List* consistencyStates)
{
   RWLock_writeLock(&this->rwlock); // L O C K

   __TargetStateStore_syncStatesFromListsUnlocked(this, targetIDs,
      reachabilityStates, consistencyStates);

   RWLock_writeUnlock(&this->rwlock); // U N L O C K
}

/**
 * Note: Caller must hold writelock.
 */
void __TargetStateStore_syncStatesFromListsUnlocked(TargetStateStore* this, UInt16List* targetIDs,
   UInt8List* reachabilityStates, UInt8List* consistencyStates)
{
   TargetStateInfoMapIter statesMapIter;
   UInt16ListIter targetIDsIter;
   UInt8ListIter reachabilityStatesIter;
   UInt8ListIter consistencyStatesIter;

   UInt16ListIter_init(&targetIDsIter, targetIDs);
   UInt8ListIter_init(&reachabilityStatesIter, reachabilityStates);
   UInt8ListIter_init(&consistencyStatesIter, consistencyStates);

   // clear map (elements have to be destructed)
   statesMapIter = TargetStateInfoMap_begin(&this->states);
   for(/* iter init'ed above */;
       !TargetStateInfoMapIter_end(&statesMapIter);
       TargetStateInfoMapIter_next(&statesMapIter) )
   {
      TargetStateInfo* stateInfo = TargetStateInfoMapIter_value(&statesMapIter);
      TargetStateInfo_destruct(stateInfo);
   }

   TargetStateInfoMap_clear(&this->states);

   for(/* iters init'ed above */;
       !UInt16ListIter_end(&targetIDsIter);
       UInt16ListIter_next(&targetIDsIter), UInt8ListIter_next(&reachabilityStatesIter),
         UInt8ListIter_next(&consistencyStatesIter) )
   {
      uint16_t currentTargetID = UInt16ListIter_value(&targetIDsIter);
      CombinedTargetState currentState = {
         (TargetReachabilityState) UInt8ListIter_value(&reachabilityStatesIter),
         (TargetConsistencyState) UInt8ListIter_value(&consistencyStatesIter)
      };

      TargetStateInfo* stateInfo = TargetStateInfo_constructFromState(currentState);

      if (unlikely(!stateInfo) )
      {
         printk_fhgfs(KERN_INFO, "%s:%d: Failed to allocate memory for TargetStateInfo; some "
            "entries could not be processed", __func__, __LINE__);
         // doesn't make sense to go further here
         break;
      }

      TargetStateInfoMap_insert(&this->states, currentTargetID, stateInfo);
   }
}

/**
 * @return pointer to static string (so don't free it)
 */
const char* TargetStateStore_reachabilityStateToStr(TargetReachabilityState state)
{
   switch(state)
   {
      case TargetReachabilityState_ONLINE:
         return "Online";

      case TargetReachabilityState_POFFLINE:
         return "Probably-offline";

      case TargetReachabilityState_OFFLINE:
         return "Offline";

      default:
         return "<invalid_value>";
   }
}

/**
 * @return pointer to static string (so don't free it)
 */
const char* TargetStateStore_consistencyStateToStr(TargetConsistencyState state)
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

void TargetStateStore_getStatesAsLists(TargetStateStore* this, UInt16List* outTargetIDs,
   UInt8List* outReachabilityStates, UInt8List* outConsistencyStates)
{
   TargetStateInfo* stateInfo = NULL;
   TargetStateInfoMapIter iter;

   RWLock_readLock(&this->rwlock); // L O C K

   for(iter = TargetStateInfoMap_begin(&this->states);
       !TargetStateInfoMapIter_end(&iter);
       TargetStateInfoMapIter_next(&iter) )
   {
      uint16_t targetID = TargetStateInfoMapIter_key(&iter);
      UInt16List_append(outTargetIDs, targetID);

      stateInfo = TargetStateInfoMapIter_value(&iter);
      UInt8List_append(outReachabilityStates, stateInfo->state.reachabilityState);
      UInt8List_append(outConsistencyStates, stateInfo->state.consistencyState);
   }

   RWLock_readUnlock(&this->rwlock); // U N L O C K
}

bool TargetStateStore_setAllStates(TargetStateStore* this, TargetReachabilityState state)
{
   TargetStateInfoMapIter statesMapIter;
   bool res = false;

   RWLock_writeLock(&this->rwlock); // L O C K

   for(statesMapIter = TargetStateInfoMap_begin(&this->states);
      !TargetStateInfoMapIter_end(&statesMapIter);
      TargetStateInfoMapIter_next(&statesMapIter) )
   {
      TargetStateInfo* stateInfo = TargetStateInfoMapIter_value(&statesMapIter);
      if(stateInfo->state.reachabilityState != state)
      {
         res = true;
         stateInfo->state.reachabilityState = state;
      }
   }

   RWLock_writeUnlock(&this->rwlock); // U N L O C K

   return res;
}
