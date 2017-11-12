#ifndef TARGETNODEMAPITER_H_
#define TARGETNODEMAPITER_H_

#include "TargetNodeMap.h"

struct TargetNodeMapIter;
typedef struct TargetNodeMapIter TargetNodeMapIter;

static inline void TargetNodeMapIter_init(TargetNodeMapIter* this, TargetNodeMap* map,
   RBTreeElem* treeElem);
static inline bool TargetNodeMapIter_hasNext(TargetNodeMapIter* this);
static inline void TargetNodeMapIter_next(TargetNodeMapIter* this);
static inline uint16_t TargetNodeMapIter_key(TargetNodeMapIter* this);
static inline NumNodeID TargetNodeMapIter_value(TargetNodeMapIter* this);
static inline bool TargetNodeMapIter_end(TargetNodeMapIter* this);

struct TargetNodeMapIter
{
   RBTreeIter rbTreeIter;
};

void TargetNodeMapIter_init(TargetNodeMapIter* this, TargetNodeMap* map, RBTreeElem* treeElem)
{
   PointerRBTreeIter_init( (RBTreeIter*)this, (RBTree*)map, (RBTreeElem*)treeElem);
}

bool TargetNodeMapIter_hasNext(TargetNodeMapIter* this)
{
   return PointerRBTreeIter_hasNext( (RBTreeIter*)this);
}

void TargetNodeMapIter_next(TargetNodeMapIter* this)
{
   PointerRBTreeIter_next( (RBTreeIter*)this);
}

uint16_t TargetNodeMapIter_key(TargetNodeMapIter* this)
{
   return (uint16_t)(size_t)PointerRBTreeIter_key( (RBTreeIter*)this);
}

NumNodeID TargetNodeMapIter_value(TargetNodeMapIter* this)
{
   return (NumNodeID){(size_t)PointerRBTreeIter_value( (RBTreeIter*)this)};
}

bool TargetNodeMapIter_end(TargetNodeMapIter* this)
{
   return PointerRBTreeIter_end( (RBTreeIter*)this);
}

#endif /* TARGETNODEMAPITER_H_ */
