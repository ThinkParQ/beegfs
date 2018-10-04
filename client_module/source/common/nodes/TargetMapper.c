#include "TargetMapper.h"

BEEGFS_RBTREE_FUNCTIONS(static, _TargetMapper, struct TargetMapper, _entries,
      uint16_t,
      struct TargetMapping, targetID, _node,
      BEEGFS_RB_KEYCMP_LT_INTEGRAL)


void TargetMapper_init(TargetMapper* this)
{
   RWLock_init(&this->rwlock);
   this->_entries = RB_ROOT;
}

TargetMapper* TargetMapper_construct(void)
{
   TargetMapper* this = (TargetMapper*)os_kmalloc(sizeof(*this) );

   TargetMapper_init(this);

   return this;
}

void TargetMapper_uninit(TargetMapper* this)
{
   BEEGFS_KFREE_RBTREE(&this->_entries, struct TargetMapping, _node);
}

void TargetMapper_destruct(TargetMapper* this)
{
   TargetMapper_uninit(this);

   kfree(this);
}

/**
 * Note: re-maps targetID if it was mapped before.
 *
 * @return true if the targetID was not mapped before
 */
bool TargetMapper_mapTarget(TargetMapper* this, uint16_t targetID,
   NumNodeID nodeID)
{
   struct TargetMapping* entry = kmalloc(sizeof(*entry), GFP_NOFS | __GFP_NOFAIL);
   struct TargetMapping* replaced;

   entry->targetID = targetID;
   entry->nodeID = nodeID;

   RWLock_writeLock(&this->rwlock);

   replaced = _TargetMapper_insertOrReplace(this, entry);

   RWLock_writeUnlock(&this->rwlock);

   kfree(replaced);
   return replaced == NULL;
}

/**
 * Applies the mapping from two separate lists (keys and values).
 *
 * @mappings list of TargetMapping objects. the list is consumed.
 *
 * Note: Does not add/remove targets from attached capacity pools.
 */
void TargetMapper_syncTargets(TargetMapper* this, struct list_head* mappings)
{
   struct TargetMapping* elem;
   struct TargetMapping* n;

   RWLock_writeLock(&this->rwlock); // L O C K

   BEEGFS_KFREE_RBTREE(&this->_entries, struct TargetMapping, _node);
   this->_entries = RB_ROOT;

   list_for_each_entry_safe(elem, n, mappings, _list)
   {
      list_del(&elem->_list);
      kfree(_TargetMapper_insertOrReplace(this, elem));
   }

   RWLock_writeUnlock(&this->rwlock); // U N L O C K
}

void TargetMapper_getTargetIDs(TargetMapper* this, UInt16List* outTargetIDs)
{
   struct rb_node* pos;

   RWLock_readLock(&this->rwlock); // L O C K

   for (pos = rb_first(&this->_entries); pos; pos = rb_next(pos))
   {
      UInt16List_append(outTargetIDs,
            rb_entry(pos, struct TargetMapping, _node)->targetID);
   }

   RWLock_readUnlock(&this->rwlock); // U N L O C K
}

/**
 * Get nodeID for a certain targetID
 *
 * @return 0 if targetID was not mapped
 */
NumNodeID TargetMapper_getNodeID(TargetMapper* this, uint16_t targetID)
{
   const struct TargetMapping* elem;
   NumNodeID nodeID;

   RWLock_readLock(&this->rwlock); // L O C K

   elem = _TargetMapper_find(this, targetID);
   if (elem)
      nodeID = elem->nodeID;
   else
      nodeID.value = 0;

   RWLock_readUnlock(&this->rwlock); // U N L O C K

   return nodeID;
}
