#include "TargetMapper.h"


void TargetMapper_init(TargetMapper* this)
{
   RWLock_init(&this->rwlock);
   TargetNodeMap_init(&this->targets);
}

TargetMapper* TargetMapper_construct(void)
{
   TargetMapper* this = (TargetMapper*)os_kmalloc(sizeof(*this) );

   TargetMapper_init(this);

   return this;
}

void TargetMapper_uninit(TargetMapper* this)
{
   TargetNodeMap_uninit(&this->targets);
   RWLock_uninit(&this->rwlock);
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
   size_t oldSize;
   size_t newSize;
   bool inserted;

   RWLock_writeLock(&this->rwlock); // L O C K

   oldSize = TargetNodeMap_length(&this->targets);

   inserted = TargetNodeMap_insert(&this->targets, targetID, nodeID);
   if(!inserted)
   { // remove old key and re-insert
      TargetNodeMap_erase(&this->targets, targetID);
      TargetNodeMap_insert(&this->targets, targetID, nodeID);
   }

   newSize = TargetNodeMap_length(&this->targets);

   RWLock_writeUnlock(&this->rwlock); // U N L O C K

   return (oldSize != newSize);
}

/**
 * @return true if the targetID was mapped
 */
bool TargetMapper_unmapTarget(TargetMapper* this, uint16_t targetID)
{
   bool keyExisted;

   RWLock_writeLock(&this->rwlock); // L O C K

   keyExisted = TargetNodeMap_erase(&this->targets, targetID);

   RWLock_writeUnlock(&this->rwlock); // U N L O C K

   return keyExisted;
}

/**
 * Applies the mapping from two separate lists (keys and values).
 *
 * Note: Does not add/remove targets from attached capacity pools.
 */
void TargetMapper_syncTargetsFromLists(TargetMapper* this, UInt16List* targetIDs,
   NumNodeIDList* nodeIDs)
{
   UInt16ListIter targetIDsIter;
   NumNodeIDListIter nodeIDsIter;

   UInt16ListIter_init(&targetIDsIter, targetIDs);
   NumNodeIDListIter_init(&nodeIDsIter, nodeIDs);

   RWLock_writeLock(&this->rwlock); // L O C K

   TargetNodeMap_clear(&this->targets);

   for(/* iters init'ed above */;
       !UInt16ListIter_end(&targetIDsIter);
       UInt16ListIter_next(&targetIDsIter), NumNodeIDListIter_next(&nodeIDsIter) )
   {
      uint16_t currentTargetID = UInt16ListIter_value(&targetIDsIter);
      NumNodeID currentNodeID = NumNodeIDListIter_value(&nodeIDsIter);

      TargetNodeMap_insert(&this->targets, currentTargetID, currentNodeID);
   }

   RWLock_writeUnlock(&this->rwlock); // U N L O C K
}

void TargetMapper_getTargetIDs(TargetMapper* this, UInt16List* outTargetIDs)
{
   TargetNodeMapIter targetsIter;

   RWLock_readLock(&this->rwlock); // L O C K

   for(targetsIter = TargetNodeMap_begin(&this->targets);
       !TargetNodeMapIter_end(&targetsIter);
       TargetNodeMapIter_next(&targetsIter) )
   {
      uint16_t currentTargetID = TargetNodeMapIter_key(&targetsIter);

      UInt16List_append(outTargetIDs, currentTargetID);
   }

   RWLock_readUnlock(&this->rwlock); // U N L O C K
}
