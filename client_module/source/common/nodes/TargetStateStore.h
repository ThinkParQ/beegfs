#ifndef TARGETSTATESTORE_H_
#define TARGETSTATESTORE_H_

#include <app/App.h>
#include <common/Common.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/threading/RWLock.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/tree/TargetStateInfoMapIter.h>
#include <common/toolkit/list/UInt8ListIter.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/toolkit/tree/UInt16MapIter.h>

struct TargetStateStore;
typedef struct TargetStateStore TargetStateStore;

extern void TargetStateStore_init(TargetStateStore* this);
extern TargetStateStore* TargetStateStore_construct(void);
extern void TargetStateStore_uninit(TargetStateStore* this);
extern void TargetStateStore_destruct(TargetStateStore* this);

extern void TargetStateStore_syncStatesAndGroupsFromLists(TargetStateStore* this, Config* config,
   MirrorBuddyGroupMapper* buddyGroups, UInt16List* targetIDs, UInt8List* reachabilityStates,
   UInt8List* consistencyStates, UInt16List* buddyGroupIDs, UInt16List* primaryTargets,
   UInt16List* secondaryTargets);
extern void TargetStateStore_syncStatesFromLists(TargetStateStore* this, UInt16List* targetIDs,
   UInt8List* reachabilityStates, UInt8List* consistencyStates);
extern void TargetStateStore_getStatesAsLists(TargetStateStore* this, UInt16List* outTargetIDs,
   UInt8List* outReachabilityStates, UInt8List* outConsistencyStates);
extern const char* TargetStateStore_reachabilityStateToStr(TargetReachabilityState state);
extern const char* TargetStateStore_consistencyStateToStr(TargetConsistencyState state);
extern bool TargetStateStore_setAllStates(TargetStateStore* this,
   TargetReachabilityState state);

extern void __TargetStateStore_syncStatesFromListsUnlocked(TargetStateStore* this,
   UInt16List* targetIDs, UInt8List* reachabilityStates, UInt8List* consistencyStates);

static inline TargetStateInfo* TargetStateStore_getStateInfo(TargetStateStore* this,
   uint16_t targetID);
static inline bool TargetStateStore_getState(TargetStateStore* this, uint16_t targetID,
   CombinedTargetState* state);
static inline unsigned TargetStateStore_getStateLastChangedSec(TargetStateStore* this,
   uint16_t targetID);

static inline TargetStateInfo* __TargetStateStore_getStateInfoUnlocked(TargetStateStore* this,
   uint16_t targetID);

struct TargetStateStore
{
   RWLock rwlock;
   TargetStateInfoMap states; // keys: targetIDs, values: targetStateInfo
};

/*
 * @return NULL, if targetID does not exist
 */
TargetStateInfo* TargetStateStore_getStateInfo(TargetStateStore* this, uint16_t targetID)
{
   TargetStateInfo* stateInfo = NULL;

   RWLock_readLock(&this->rwlock); // L O C K

   stateInfo = __TargetStateStore_getStateInfoUnlocked(this, targetID);

   RWLock_readUnlock(&this->rwlock); // U N L O C K

   return stateInfo;
}

bool TargetStateStore_getState(TargetStateStore* this, uint16_t targetID,
   CombinedTargetState* state)
{
   TargetStateInfo* stateInfo;
   bool res;

   RWLock_readLock(&this->rwlock); // L O C K

   stateInfo = __TargetStateStore_getStateInfoUnlocked(this, targetID);

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

unsigned TargetStateStore_getStateLastChangedSec(TargetStateStore* this, uint16_t targetID)
{
   TargetStateInfo* stateInfo;
   unsigned lastChangedSec;

   RWLock_readLock(&this->rwlock); // L O C K

   stateInfo = __TargetStateStore_getStateInfoUnlocked(this, targetID);

   if(likely(stateInfo) )
      lastChangedSec = Time_elapsedMS(&stateInfo->lastChangedTime);
   else
      lastChangedSec = 0;

   RWLock_readUnlock(&this->rwlock); // U N L O C K

   return lastChangedSec;
}

/*
 * @return NULL, if targetID does not exist
 */
TargetStateInfo* __TargetStateStore_getStateInfoUnlocked(TargetStateStore* this, uint16_t targetID)
{
   TargetStateInfo* stateInfo = NULL;
   TargetStateInfoMapIter iter;

   iter = TargetStateInfoMap_find(&this->states, targetID);

   if(likely(!TargetStateInfoMapIter_end(&iter)))
      stateInfo = TargetStateInfoMapIter_value(&iter);

   return stateInfo;
}

#endif /* TARGETSTATESTORE_H_ */
