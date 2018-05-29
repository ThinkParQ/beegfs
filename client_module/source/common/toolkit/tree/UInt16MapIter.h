#ifndef UINT16MAPITER_H_
#define UINT16MAPITER_H_

#include "UInt16Map.h"

struct UInt16MapIter;
typedef struct UInt16MapIter UInt16MapIter;

static inline void UInt16MapIter_init(UInt16MapIter* this, UInt16Map* map, RBTreeElem* treeElem);
static inline bool UInt16MapIter_hasNext(UInt16MapIter* this);
static inline void UInt16MapIter_next(UInt16MapIter* this);
static inline uint16_t UInt16MapIter_key(UInt16MapIter* this);
static inline uint16_t UInt16MapIter_value(UInt16MapIter* this);
static inline bool UInt16MapIter_end(UInt16MapIter* this);

struct UInt16MapIter
{
   RBTreeIter rbTreeIter;
};

void UInt16MapIter_init(UInt16MapIter* this, UInt16Map* map, RBTreeElem* treeElem)
{
   PointerRBTreeIter_init( (RBTreeIter*)this, (RBTree*)map, (RBTreeElem*)treeElem);
}

bool UInt16MapIter_hasNext(UInt16MapIter* this)
{
   return PointerRBTreeIter_hasNext( (RBTreeIter*)this);
}

void UInt16MapIter_next(UInt16MapIter* this)
{
   PointerRBTreeIter_next( (RBTreeIter*)this);
}

uint16_t UInt16MapIter_key(UInt16MapIter* this)
{
   return (uint16_t)(size_t)PointerRBTreeIter_key( (RBTreeIter*)this);
}

uint16_t UInt16MapIter_value(UInt16MapIter* this)
{
   return (uint16_t)(size_t)PointerRBTreeIter_value( (RBTreeIter*)this);
}

bool UInt16MapIter_end(UInt16MapIter* this)
{
   return PointerRBTreeIter_end( (RBTreeIter*)this);
}

#endif /* UINT16MAPITER_H_ */
