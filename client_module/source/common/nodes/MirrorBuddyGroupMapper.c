#include <common/nodes/TargetMapper.h>

#include "MirrorBuddyGroupMapper.h"

BEEGFS_RBTREE_FUNCTIONS(static, _MirrorBuddyGroupMapper, struct MirrorBuddyGroupMapper,
      mirrorBuddyGroups,
      uint16_t,
      struct MirrorBuddyGroup, groupID, _rb_node,
      BEEGFS_RB_KEYCMP_LT_INTEGRAL)

void MirrorBuddyGroupMapper_init(MirrorBuddyGroupMapper* this)
{
   RWLock_init(&this->rwlock);
   this->mirrorBuddyGroups = RB_ROOT;
}

static void _MirrorBuddyGroupMapper_clear(MirrorBuddyGroupMapper* this)
{
   MirrorBuddyGroup* pos;
   MirrorBuddyGroup* n;

   rbtree_postorder_for_each_entry_safe(pos, n, &this->mirrorBuddyGroups, _rb_node)
      MirrorBuddyGroup_put(pos);

   this->mirrorBuddyGroups = RB_ROOT;
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
   _MirrorBuddyGroupMapper_clear(this);
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

   buddyGroup = _MirrorBuddyGroupMapper_find(this, mirrorBuddyGroupID);

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

   buddyGroup = _MirrorBuddyGroupMapper_find(this, mirrorBuddyGroupID);
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

   buddyGroup = _MirrorBuddyGroupMapper_find(this, mirrorBuddyGroupID);
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
void MirrorBuddyGroupMapper_syncGroups(MirrorBuddyGroupMapper* this,
   Config* config, struct list_head* groups)
{
   RWLock_writeLock(&this->rwlock); // L O C K

   __MirrorBuddyGroupMapper_syncGroupsUnlocked(this, config, groups);

   RWLock_writeUnlock(&this->rwlock); // U N L O C K
}


/**
 * note: caller must hold writelock.
 */
void __MirrorBuddyGroupMapper_syncGroupsUnlocked(MirrorBuddyGroupMapper* this,
   Config* config, struct list_head* groups)
{
   struct BuddyGroupMapping* mapping;

   LIST_HEAD(newGroups);

   list_for_each_entry(mapping, groups, _list)
   {
      MirrorBuddyGroup* group;

      // if the group exists already, update it and move it over to the new tree
      group = _MirrorBuddyGroupMapper_find(this, mapping->groupID);
      if (group)
      {
         group->firstTargetID = mapping->primaryTargetID;
         group->secondTargetID = mapping->secondaryTargetID;

         _MirrorBuddyGroupMapper_erase(this, group);
         list_add_tail(&group->_list, &newGroups);
      }
      else
      {
         group = MirrorBuddyGroup_constructFromTargetIDs(mapping->groupID,
               config->connMaxInternodeNum, mapping->primaryTargetID, mapping->secondaryTargetID);

         list_add_tail(&group->_list, &newGroups);
      }
   }

   _MirrorBuddyGroupMapper_clear(this);

   {
      MirrorBuddyGroup* pos;
      MirrorBuddyGroup* tmp;

      list_for_each_entry_safe(pos, tmp, &newGroups, _list)
      {
         MirrorBuddyGroup* replaced = _MirrorBuddyGroupMapper_insertOrReplace(this, pos);
         if (replaced)
            MirrorBuddyGroup_put(replaced);
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
      MirrorBuddyGroup* group = _MirrorBuddyGroupMapper_find(this, buddyGroupID);

      if (group)
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
   buddyGroup = MirrorBuddyGroup_constructFromTargetIDs(buddyGroupID, config->connMaxInternodeNum,
         primaryTargetID, secondaryTargetID);

   if (unlikely(!buddyGroup) )
   {
      printk_fhgfs(KERN_INFO, "%s:%d: Failed to allocate memory for MirrorBuddyGroup.",
         __func__, __LINE__);
      res = FhgfsOpsErr_OUTOFMEM;
      goto unlock;
   }

   _MirrorBuddyGroupMapper_insert(this, buddyGroup);

unlock:
   RWLock_writeUnlock(&this->rwlock); // U N L O C K

   return res;
}


uint16_t __MirrorBuddyGroupMapper_getBuddyGroupIDUnlocked(MirrorBuddyGroupMapper* this,
   uint16_t targetID)
{
   MirrorBuddyGroup* buddyGroup;
   BEEGFS_RBTREE_FOR_EACH_ENTRY(buddyGroup, &this->mirrorBuddyGroups, _rb_node)
   {
      if (buddyGroup->firstTargetID == targetID || buddyGroup->secondTargetID == targetID)
      {
         return buddyGroup->groupID;
      }
   }

   return 0;
}
