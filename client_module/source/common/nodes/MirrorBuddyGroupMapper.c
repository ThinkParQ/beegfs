#include <common/nodes/TargetMapper.h>

#include "MirrorBuddyGroupMapper.h"

void MirrorBuddyGroupMapper_init(MirrorBuddyGroupMapper* this)
{
   RWLock_init(&this->rwlock);

   MirrorBuddyGroupMap_init(&this->mirrorBuddyGroups);
}

MirrorBuddyGroupMapper* MirrorBuddyGroupMapper_construct(void)
{
   MirrorBuddyGroupMapper* this = (MirrorBuddyGroupMapper*)os_kmalloc(sizeof(*this) );

   if (likely(this))
      MirrorBuddyGroupMapper_init(this);

   return this;
}

void MirrorBuddyGroupMapper_uninit(MirrorBuddyGroupMapper* this)
{
   // clear map (elements have to be destructed)
   MirrorBuddyGroupMapIter buddyGroupMapIter;

   buddyGroupMapIter = MirrorBuddyGroupMap_begin(&this->mirrorBuddyGroups);
   for(/* iter init'ed above */;
       !MirrorBuddyGroupMapIter_end(&buddyGroupMapIter);
       MirrorBuddyGroupMapIter_next(&buddyGroupMapIter) )
   {
      MirrorBuddyGroup* buddyGroup = MirrorBuddyGroupMapIter_value(&buddyGroupMapIter);
      MirrorBuddyGroup_put(buddyGroup);
   }

   MirrorBuddyGroupMap_uninit(&this->mirrorBuddyGroups);

   RWLock_uninit(&this->rwlock);
}

void MirrorBuddyGroupMapper_destruct(MirrorBuddyGroupMapper* this)
{
   MirrorBuddyGroupMapper_uninit(this);

   kfree(this);
}

/**
 * @return 0 if group ID not found
 */
uint16_t MirrorBuddyGroupMapper_getPrimaryTargetID(MirrorBuddyGroupMapper* this,
   uint16_t mirrorBuddyGroupID)
{
   MirrorBuddyGroup* buddyGroup;
   uint16_t targetID;

   RWLock_readLock(&this->rwlock); // L O C K

   buddyGroup = __MirrorBuddyGroupMapper_getMirrorBuddyGroupUnlocked(this, mirrorBuddyGroupID);

   if(likely(buddyGroup))
      targetID = buddyGroup->firstTargetID;
   else
      targetID = 0;

   RWLock_readUnlock(&this->rwlock); // U N L O C K

   return targetID;
}

/**
 * @return 0 if group ID not found
 */
uint16_t MirrorBuddyGroupMapper_getSecondaryTargetID(MirrorBuddyGroupMapper* this,
   uint16_t mirrorBuddyGroupID)
{
   MirrorBuddyGroup* buddyGroup;
   uint16_t targetID;

   RWLock_readLock(&this->rwlock); // L O C K

   buddyGroup = __MirrorBuddyGroupMapper_getMirrorBuddyGroupUnlocked(this, mirrorBuddyGroupID);

   if(likely(buddyGroup))
      targetID = buddyGroup->secondTargetID;
   else
      targetID = 0;

   RWLock_readUnlock(&this->rwlock); // U N L O C K

   return targetID;
}

int MirrorBuddyGroupMapper_acquireSequenceNumber(MirrorBuddyGroupMapper* this,
   uint16_t mirrorBuddyGroupID, uint64_t* seqNo, uint64_t* finishedSeqNo, bool* isSelective,
   struct BuddySequenceNumber** handle, struct MirrorBuddyGroup** group)
{
   MirrorBuddyGroup* buddyGroup;
   int result = 0;

   RWLock_readLock(&this->rwlock);

   buddyGroup = __MirrorBuddyGroupMapper_getMirrorBuddyGroupUnlocked(this, mirrorBuddyGroupID);
   if (!buddyGroup)
   {
      RWLock_readUnlock(&this->rwlock);
      return ENOENT;
   }

   *group = buddyGroup;

   result = MirrorBuddyGroup_acquireSequenceNumber(buddyGroup, finishedSeqNo, isSelective, seqNo,
         handle, false);
   // treat ENOENT (no seqNoBase set) as success. the target node will reply with a generic response
   // that sets the seqNoBase for this buddy group.
   if (result == 0 || result == ENOENT)
   {
      RWLock_readUnlock(&this->rwlock);
      return 0;
   }

   // nowait acquire failed, need to wait for a free slot. get a ref, unlock this, and go on
   kref_get(&buddyGroup->refs);

   RWLock_readUnlock(&this->rwlock);

   result = MirrorBuddyGroup_acquireSequenceNumber(buddyGroup, finishedSeqNo, isSelective, seqNo,
         handle, true);
   MirrorBuddyGroup_put(buddyGroup);
   // treat ENOENT as success here, too. we will hopefully never have to wait for a sequence number
   // on such a partially initialized state, but it may happen under very high load.
   return result == ENOENT ? 0 : result;
}

/**
 * NOTE: no sanity checks in here
 */
void MirrorBuddyGroupMapper_syncGroupsFromLists(MirrorBuddyGroupMapper* this,
   Config* config, UInt16List* buddyGroupIDs, UInt16List* primaryTargets,
   UInt16List* secondaryTargets)
{
   RWLock_writeLock(&this->rwlock); // L O C K

   __MirrorBuddyGroupMapper_syncGroupsFromListsUnlocked(this, config, buddyGroupIDs,
      primaryTargets, secondaryTargets);

   RWLock_writeUnlock(&this->rwlock); // U N L O C K
}


/**
 * note: caller must hold writelock.
 */
void __MirrorBuddyGroupMapper_syncGroupsFromListsUnlocked(MirrorBuddyGroupMapper* this,
   Config* config, UInt16List* buddyGroupIDs, UInt16List* primaryTargets,
   UInt16List* secondaryTargets)
{
   UInt16ListIter buddyGroupIDsIter;
   UInt16ListIter primaryTargetsIter;
   UInt16ListIter secondaryTargetsIter;

   MirrorBuddyGroupMapIter buddyGroupMapIter;

   UInt16ListIter_init(&buddyGroupIDsIter, buddyGroupIDs);
   UInt16ListIter_init(&primaryTargetsIter, primaryTargets);
   UInt16ListIter_init(&secondaryTargetsIter, secondaryTargets);

   // set all known groups to unmarked. we will later delete all groups that have not been marked
   // by the sync.
   buddyGroupMapIter = MirrorBuddyGroupMap_begin(&this->mirrorBuddyGroups);
   for(/* iter init'ed above */;
       !MirrorBuddyGroupMapIter_end(&buddyGroupMapIter);
       MirrorBuddyGroupMapIter_next(&buddyGroupMapIter) )
   {
      MirrorBuddyGroupMapIter_value(&buddyGroupMapIter)->marked = false;
   }

   for(/* iters init'ed above */;
       !UInt16ListIter_end(&buddyGroupIDsIter);
       UInt16ListIter_next(&buddyGroupIDsIter), UInt16ListIter_next(&primaryTargetsIter),
          UInt16ListIter_next(&secondaryTargetsIter) )
   {
      uint16_t currentBuddyGroupID = UInt16ListIter_value(&buddyGroupIDsIter);
      uint16_t currentPrimaryTargetID = UInt16ListIter_value(&primaryTargetsIter);
      uint16_t currentSecondaryTargetID = UInt16ListIter_value(&secondaryTargetsIter);

      MirrorBuddyGroup* group;

      // if the group exists already, only set the primary/secondary IDs (should they have changed)
      group = __MirrorBuddyGroupMapper_getMirrorBuddyGroupUnlocked(this, currentBuddyGroupID);
      if (group)
      {
         group->marked = true;
         group->firstTargetID = currentPrimaryTargetID;
         group->secondTargetID = currentSecondaryTargetID;
         continue;
      }

      group = MirrorBuddyGroup_constructFromTargetIDs(config->connMaxInternodeNum,
            currentPrimaryTargetID, currentSecondaryTargetID);

      if(unlikely(!group))
      {
         printk_fhgfs(KERN_INFO, "%s:%d: Failed to allocate memory for MirrorBuddyGroup; some "
            "entries could not be processed", __func__, __LINE__);
         // doesn't make sense to go further here
         break;
      }

      group->marked = true;
      MirrorBuddyGroupMap_insert(&this->mirrorBuddyGroups, currentBuddyGroupID, group);
   }

   // remove all unmarked (aka deleted) groups from the map
   buddyGroupMapIter = MirrorBuddyGroupMap_begin(&this->mirrorBuddyGroups);
   while (!MirrorBuddyGroupMapIter_end(&buddyGroupMapIter))
   {
      MirrorBuddyGroup* group = MirrorBuddyGroupMapIter_value(&buddyGroupMapIter);
      uint16_t key = MirrorBuddyGroupMapIter_key(&buddyGroupMapIter);

      MirrorBuddyGroupMapIter_next(&buddyGroupMapIter);

      if (!group->marked)
      {
         MirrorBuddyGroupMap_erase(&this->mirrorBuddyGroups, key);
         MirrorBuddyGroup_put(group);
      }
   }
}

/**
 * Adds a new buddy group to the map.
 * @param targetMapper global targetmapper; must be set for storage nodes (and only for storage)
 * @param buddyGroupID The ID of the new buddy group. Must be non-zero.
 * @param primaryTargetID
 * @param secondaryTargetID
 * @param allowUpdate Allow updating an existing buddy group.
 */
FhgfsOpsErr MirrorBuddyGroupMapper_addGroup(MirrorBuddyGroupMapper* this,
   Config* config, TargetMapper* targetMapper, uint16_t buddyGroupID, uint16_t primaryTargetID,
   uint16_t secondaryTargetID, bool allowUpdate)
{
   FhgfsOpsErr res = FhgfsOpsErr_SUCCESS;

   uint16_t primaryInGroup;
   uint16_t secondaryInGroup;
   MirrorBuddyGroup* buddyGroup;
   NumNodeID primaryNodeID;
   NumNodeID secondaryNodeID;

   // ID = 0 is an error.
   if (buddyGroupID == 0)
      return FhgfsOpsErr_INVAL;

   RWLock_writeLock(&this->rwlock); // L O C K

   if (!allowUpdate)
   {
      // If group already exists return error.
      MirrorBuddyGroupMapIter iter =
         MirrorBuddyGroupMap_find(&this->mirrorBuddyGroups, buddyGroupID);

      if (!MirrorBuddyGroupMapIter_end(&iter) )
      {
         res = FhgfsOpsErr_EXISTS;
         goto unlock;
      }
   }

   // Check if both targets exist for storage nodes
   if (targetMapper)
   {
      primaryNodeID = TargetMapper_getNodeID(targetMapper, primaryTargetID);
      secondaryNodeID = TargetMapper_getNodeID(targetMapper, secondaryTargetID);
      if(NumNodeID_isZero(&primaryNodeID) || NumNodeID_isZero(&secondaryNodeID))
      {
         res = FhgfsOpsErr_UNKNOWNTARGET;
         goto unlock;
      }
   }

   // Check that both targets are not yet part of any group.
   primaryInGroup = __MirrorBuddyGroupMapper_getBuddyGroupIDUnlocked(this, primaryTargetID);
   secondaryInGroup = __MirrorBuddyGroupMapper_getBuddyGroupIDUnlocked(this, secondaryTargetID);

   if (  ( (primaryInGroup != 0) && (primaryInGroup != buddyGroupID) )
      || ( (secondaryInGroup != 0) && (secondaryInGroup != buddyGroupID) ) )
   {
      res = FhgfsOpsErr_INUSE;
      goto unlock;
   }

   // Create and insert new mirror buddy group.
   buddyGroup = MirrorBuddyGroup_constructFromTargetIDs(config->connMaxInternodeNum,
         primaryTargetID, secondaryTargetID);

   if (unlikely(!buddyGroup) )
   {
      printk_fhgfs(KERN_INFO, "%s:%d: Failed to allocate memory for MirrorBuddyGroup.",
         __func__, __LINE__);
      res = FhgfsOpsErr_OUTOFMEM;
      goto unlock;
   }

   MirrorBuddyGroupMap_insert(&this->mirrorBuddyGroups, buddyGroupID, buddyGroup);

unlock:
   RWLock_writeUnlock(&this->rwlock); // U N L O C K

   return res;
}


/**
 * @return a pointer to the buddy group in the map or NULL if key does not exist
 *
 * NOTE: no locks, so caller should hold an RWLock
 */
MirrorBuddyGroup* __MirrorBuddyGroupMapper_getMirrorBuddyGroupUnlocked(MirrorBuddyGroupMapper* this,
   uint16_t mirrorBuddyGroupID)
{
   MirrorBuddyGroup* buddyGroup = NULL;

   MirrorBuddyGroupMapIter iter;

   iter = MirrorBuddyGroupMap_find(&this->mirrorBuddyGroups, mirrorBuddyGroupID);
   if(likely(!MirrorBuddyGroupMapIter_end(&iter)))
      buddyGroup = MirrorBuddyGroupMapIter_value(&iter);

   return buddyGroup;
}

uint16_t __MirrorBuddyGroupMapper_getBuddyGroupIDUnlocked(MirrorBuddyGroupMapper* this,
   uint16_t targetID)
{
   uint16_t buddyGroupID = 0;
   MirrorBuddyGroupMapIter buddyGroupMapIter;

   for (buddyGroupMapIter = MirrorBuddyGroupMap_begin(&this->mirrorBuddyGroups);
        !MirrorBuddyGroupMapIter_end(&buddyGroupMapIter);
        MirrorBuddyGroupMapIter_next(&buddyGroupMapIter) )
   {
      MirrorBuddyGroup* buddyGroup = MirrorBuddyGroupMapIter_value(&buddyGroupMapIter);
      if (buddyGroup->firstTargetID == targetID || buddyGroup->secondTargetID == targetID)
      {
         targetID = MirrorBuddyGroupMapIter_key(&buddyGroupMapIter);
         break;
      }
   }

   return buddyGroupID;
}
