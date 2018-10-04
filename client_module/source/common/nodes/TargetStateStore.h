#ifndef TARGETSTATESTORE_H_
#define TARGETSTATESTORE_H_

#include <app/App.h>
#include <common/Common.h>
#include <common/Types.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/threading/RWLock.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/list/UInt8ListIter.h>
#include <common/toolkit/list/UInt16ListIter.h>

struct TargetStateStore;
typedef struct TargetStateStore TargetStateStore;

extern void TargetStateStore_init(TargetStateStore* this);
extern TargetStateStore* TargetStateStore_construct(void);
extern void TargetStateStore_uninit(TargetStateStore* this);
extern void TargetStateStore_destruct(TargetStateStore* this);

extern void TargetStateStore_syncStatesAndGroupsFromLists(TargetStateStore* this, Config* config,
   MirrorBuddyGroupMapper* buddyGroups, struct list_head* states, struct list_head* groups);
extern void TargetStateStore_getStatesAsLists(TargetStateStore* this, UInt16List* outTargetIDs,
   UInt8List* outReachabilityStates, UInt8List* outConsistencyStates);
extern const char* TargetStateStore_reachabilityStateToStr(TargetReachabilityState state);
extern const char* TargetStateStore_consistencyStateToStr(TargetConsistencyState state);
extern bool TargetStateStore_setAllStates(TargetStateStore* this,
   TargetReachabilityState state);

extern void __TargetStateStore_syncStatesUnlocked(TargetStateStore* this, struct list_head* states);

extern bool TargetStateStore_getState(TargetStateStore* this, uint16_t targetID,
   CombinedTargetState* state);

struct TargetStateStore
{
   RWLock rwlock;
   struct rb_root states; /* struct TargetStateInfo */
};

#endif /* TARGETSTATESTORE_H_ */
