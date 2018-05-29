#ifndef TARGETMAPPER_H_
#define TARGETMAPPER_H_

#include <app/App.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/toolkit/tree/TargetNodeMapIter.h>
#include <common/toolkit/list/NumNodeIDListIter.h>
#include <common/Common.h>
#include <common/threading/RWLock.h>
#include <common/toolkit/StringTk.h>


struct TargetMapper;
typedef struct TargetMapper TargetMapper;


extern void TargetMapper_init(TargetMapper* this);
extern TargetMapper* TargetMapper_construct(void);
extern void TargetMapper_uninit(TargetMapper* this);
extern void TargetMapper_destruct(TargetMapper* this);

extern bool TargetMapper_mapTarget(TargetMapper* this, uint16_t targetID,
   NumNodeID nodeID);
extern bool TargetMapper_unmapTarget(TargetMapper* this, uint16_t targetID);

extern void TargetMapper_syncTargetsFromLists(TargetMapper* this, UInt16List* targetIDs,
   NumNodeIDList* nodeIDs);
extern void TargetMapper_getTargetIDs(TargetMapper* this, UInt16List* outTargetIDs);

// getters & setters
static inline NumNodeID TargetMapper_getNodeID(TargetMapper* this, uint16_t targetID);


struct TargetMapper
{
   RWLock rwlock;
   TargetNodeMap targets; // keys: targetIDs, values: nodeIDs
};


/**
 * Get nodeID for a certain targetID
 *
 * @return 0 if targetID was not mapped
 */
NumNodeID TargetMapper_getNodeID(TargetMapper* this, uint16_t targetID)
{
   NumNodeID nodeID;
   TargetNodeMapIter iter;

   RWLock_readLock(&this->rwlock); // L O C K

   iter = TargetNodeMap_find(&this->targets, targetID);
   if(likely(!TargetNodeMapIter_end(&iter) ) )
      nodeID = TargetNodeMapIter_value(&iter);
   else
      nodeID.value = 0;

   RWLock_readUnlock(&this->rwlock); // U N L O C K

   return nodeID;
}

#endif /* TARGETMAPPER_H_ */
