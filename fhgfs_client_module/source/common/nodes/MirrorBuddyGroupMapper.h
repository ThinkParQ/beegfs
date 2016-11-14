#ifndef MIRRORBUDDYGROUPMAPPER_H_
#define MIRRORBUDDYGROUPMAPPER_H_

#include <app/config/Config.h>
#include <common/nodes/TargetMapper.h>
#include <common/threading/RWLock.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/toolkit/tree/MirrorBuddyGroupMapIter.h>
#include <common/toolkit/StringTk.h>
#include <common/storage/StorageErrors.h>

struct MirrorBuddyGroupMapper;
typedef struct MirrorBuddyGroupMapper MirrorBuddyGroupMapper;

extern void MirrorBuddyGroupMapper_init(MirrorBuddyGroupMapper* this);
extern MirrorBuddyGroupMapper* MirrorBuddyGroupMapper_construct(void);
extern void MirrorBuddyGroupMapper_uninit(MirrorBuddyGroupMapper* this);
extern void MirrorBuddyGroupMapper_destruct(MirrorBuddyGroupMapper* this);

extern uint16_t MirrorBuddyGroupMapper_getPrimaryTargetID(MirrorBuddyGroupMapper* this,
   uint16_t mirrorBuddyGroupID);
extern uint16_t MirrorBuddyGroupMapper_getSecondaryTargetID(MirrorBuddyGroupMapper* this,
   uint16_t mirrorBuddyGroupID);
extern int MirrorBuddyGroupMapper_acquireSequenceNumber(MirrorBuddyGroupMapper* this,
   uint16_t mirrorBuddyGroupID, uint64_t* seqNo, uint64_t* finishedSeqNo, bool* isSelective,
   struct BuddySequenceNumber** handle, struct MirrorBuddyGroup** group);

extern void MirrorBuddyGroupMapper_syncGroupsFromLists(MirrorBuddyGroupMapper* this,
   Config* config, UInt16List* buddyGroupIDs, UInt16List* primaryTargets,
   UInt16List* secondaryTargets);
extern FhgfsOpsErr MirrorBuddyGroupMapper_addGroup(MirrorBuddyGroupMapper* this,
   Config* config, TargetMapper* targetMapper, uint16_t buddyGroupID, uint16_t primaryTargetID,
   uint16_t secondaryTargetID, bool allowUpdate);

extern MirrorBuddyGroup* __MirrorBuddyGroupMapper_getMirrorBuddyGroupUnlocked(
   MirrorBuddyGroupMapper* this, uint16_t mirrorBuddyGroupID);
extern uint16_t __MirrorBuddyGroupMapper_getBuddyGroupIDUnlocked(MirrorBuddyGroupMapper* this,
   uint16_t targetID);
extern void __MirrorBuddyGroupMapper_syncGroupsFromListsUnlocked(MirrorBuddyGroupMapper* this,
   Config* config, UInt16List* buddyGroupIDs, UInt16List* primaryTargets,
   UInt16List* secondaryTargets);

struct MirrorBuddyGroupMapper
{
   // friend class TargetStateStore; // for atomic update of state change plus mirror group switch

   RWLock rwlock;
   MirrorBuddyGroupMap mirrorBuddyGroups;
};

#endif /* MIRRORBUDDYGROUPMAPPER_H_ */
