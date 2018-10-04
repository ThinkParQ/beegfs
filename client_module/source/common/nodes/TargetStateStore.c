#include "TargetStateStore.h"

BEEGFS_RBTREE_FUNCTIONS(static, _TargetStateStore, struct TargetStateStore, states,
      uint16_t,
      struct TargetStateInfo, targetID, _node,
      BEEGFS_RB_KEYCMP_LT_INTEGRAL)

void TargetStateStore_init(TargetStateStore* this)
{
   RWLock_init(&this->rwlock);
   this->states = RB_ROOT;
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
   BEEGFS_KFREE_RBTREE(&this->states, struct TargetStateInfo, _node);
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
   MirrorBuddyGroupMapper* buddyGroups, struct list_head* states, struct list_head* groups)
{
   RWLock_writeLock(&this->rwlock); // L O C K targetStates
   RWLock_writeLock(&buddyGroups->rwlock); // L O C K buddyGroups

   __TargetStateStore_syncStatesUnlocked(this, states);

   __MirrorBuddyGroupMapper_syncGroupsUnlocked(buddyGroups, config, groups);

   RWLock_writeUnlock(&buddyGroups->rwlock); // U N L O C K buddyGroups
   RWLock_writeUnlock(&this->rwlock); // U N L O C K targetStates
}

/**
 * Note: Caller must hold writelock.
 */
void __TargetStateStore_syncStatesUnlocked(TargetStateStore* this, struct list_head* states)
{
   struct TargetStateMapping* state;

   // clear existing map
   BEEGFS_KFREE_RBTREE(&this->states, struct TargetStateInfo, _node);

   list_for_each_entry(state, states, _list)
   {
      CombinedTargetState currentState = { state->reachabilityState, state->consistencyState };

      TargetStateInfo* stateInfo = kmalloc(sizeof(TargetStateInfo), GFP_NOFS | __GFP_NOFAIL);

      stateInfo->targetID = state->targetID;
      stateInfo->state = currentState;

      kfree(_TargetStateStore_insertOrReplace(this, stateInfo));
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
   struct TargetStateInfo* stateInfo;
   RWLock_readLock(&this->rwlock); // L O C K

   BEEGFS_RBTREE_FOR_EACH_ENTRY(stateInfo, &this->states, _node)
   {
      UInt16List_append(outTargetIDs, stateInfo->targetID);
      UInt8List_append(outReachabilityStates, stateInfo->state.reachabilityState);
      UInt8List_append(outConsistencyStates, stateInfo->state.consistencyState);
   }

   RWLock_readUnlock(&this->rwlock); // U N L O C K
}

bool TargetStateStore_setAllStates(TargetStateStore* this, TargetReachabilityState state)
{
   bool res = false;
   struct TargetStateInfo* stateInfo;

   RWLock_writeLock(&this->rwlock); // L O C K

   BEEGFS_RBTREE_FOR_EACH_ENTRY(stateInfo, &this->states, _node)
   {
      if (stateInfo->state.reachabilityState != state)
      {
         res = true;
         stateInfo->state.reachabilityState = state;
      }
   }

   RWLock_writeUnlock(&this->rwlock); // U N L O C K

   return res;
}

bool TargetStateStore_getState(TargetStateStore* this, uint16_t targetID,
   CombinedTargetState* state)
{
   TargetStateInfo* stateInfo;
   bool res;

   RWLock_readLock(&this->rwlock); // L O C K

   stateInfo = _TargetStateStore_find(this, targetID);

   if(likely(stateInfo) )
   {
      *state = stateInfo->state;
      res = true;
   }
   else
   {
      state->reachabilityState = TargetReachabilityState_OFFLINE;
      state->consistencyState = TargetConsistencyState_GOOD;
      res = false;
   }

   RWLock_readUnlock(&this->rwlock); // U N L O C K

   return res;
}
