#ifndef INTMAPITER_H_
#define INTMAPITER_H_

#include "IntMap.h"

struct IntMapIter;
typedef struct IntMapIter IntMapIter;

static inline void IntMapIter_init(IntMapIter* this, IntMap* map, RBTreeElem* treeElem);
static inline bool IntMapIter_hasNext(IntMapIter* this);
static inline char* IntMapIter_next(IntMapIter* this);
static inline int IntMapIter_key(IntMapIter* this);
static inline char* IntMapIter_value(IntMapIter* this);
static inline bool IntMapIter_end(IntMapIter* this);

struct IntMapIter
{
   RBTreeIter rbTreeIter;
};

void IntMapIter_init(IntMapIter* this, IntMap* map, RBTreeElem* treeElem)
{
   PointerRBTreeIter_init( (RBTreeIter*)this, (RBTree*)map, (RBTreeElem*)treeElem);
}
bool IntMapIter_hasNext(IntMapIter* this)
{
   return PointerRBTreeIter_hasNext( (RBTreeIter*)this);
}

char* IntMapIter_next(IntMapIter* this)
{
   return (char*)PointerRBTreeIter_next( (RBTreeIter*)this);
}

int IntMapIter_key(IntMapIter* this)
{
   return (int)(size_t)PointerRBTreeIter_key( (RBTreeIter*)this);
}

char* IntMapIter_value(IntMapIter* this)
{
   return (char*)PointerRBTreeIter_value( (RBTreeIter*)this);
}

bool IntMapIter_end(IntMapIter* this)
{
   return PointerRBTreeIter_end( (RBTreeIter*)this);
}

#endif /* INTMAPITER_H_ */
