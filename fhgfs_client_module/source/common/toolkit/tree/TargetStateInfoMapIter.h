#ifndef TARGETSTATEINFOMAPITER_H_
#define TARGETSTATEINFOMAPITER_H_

#include "TargetStateInfoMap.h"

struct TargetStateInfoMapIter;
typedef struct TargetStateInfoMapIter TargetStateInfoMapIter;

static inline void TargetStateInfoMapIter_init(TargetStateInfoMapIter* this,
   TargetStateInfoMap* map, RBTreeElem* treeElem);
static inline bool TargetStateInfoMapIter_hasNext(TargetStateInfoMapIter* this);
static inline void TargetStateInfoMapIter_next(TargetStateInfoMapIter* this);
static inline uint16_t TargetStateInfoMapIter_key(TargetStateInfoMapIter* this);
static inline TargetStateInfo* TargetStateInfoMapIter_value(TargetStateInfoMapIter* this);
static inline bool TargetStateInfoMapIter_end(TargetStateInfoMapIter* this);

struct TargetStateInfoMapIter
{
   RBTreeIter rbTreeIter;
};

void TargetStateInfoMapIter_init(TargetStateInfoMapIter* this, TargetStateInfoMap* map,
   RBTreeElem* treeElem)
{
   PointerRBTreeIter_init((RBTreeIter*) this, (RBTree*) map, (RBTreeElem*) treeElem);
}

bool TargetStateInfoMapIter_hasNext(TargetStateInfoMapIter* this)
{
   return PointerRBTreeIter_hasNext((RBTreeIter*) this);
}

void TargetStateInfoMapIter_next(TargetStateInfoMapIter* this)
{
   PointerRBTreeIter_next((RBTreeIter*) this);
}

uint16_t TargetStateInfoMapIter_key(TargetStateInfoMapIter* this)
{
   return (uint16_t) (size_t) PointerRBTreeIter_key((RBTreeIter*) this);
}

TargetStateInfo* TargetStateInfoMapIter_value(TargetStateInfoMapIter* this)
{
   return (TargetStateInfo*) (size_t) PointerRBTreeIter_value((RBTreeIter*) this);
}

bool TargetStateInfoMapIter_end(TargetStateInfoMapIter* this)
{
   return PointerRBTreeIter_end((RBTreeIter*) this);
}

#endif /* TARGETSTATEINFOMAPITER_H_ */
