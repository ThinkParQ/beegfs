#ifndef WAITACKMAPITER_H_
#define WAITACKMAPITER_H_

#include "WaitAckMap.h"

struct WaitAckMapIter;
typedef struct WaitAckMapIter WaitAckMapIter;

static inline void WaitAckMapIter_init(WaitAckMapIter* this, WaitAckMap* map, RBTreeElem* treeElem);
static inline WaitAck* WaitAckMapIter_next(WaitAckMapIter* this);
static inline char* WaitAckMapIter_key(WaitAckMapIter* this);
static inline WaitAck* WaitAckMapIter_value(WaitAckMapIter* this);
static inline bool WaitAckMapIter_end(WaitAckMapIter* this);

struct WaitAckMapIter
{
   RBTreeIter rbTreeIter;
};

void WaitAckMapIter_init(WaitAckMapIter* this, WaitAckMap* map, RBTreeElem* treeElem)
{
   PointerRBTreeIter_init( (RBTreeIter*)this, (RBTree*)map, (RBTreeElem*)treeElem);
}

WaitAck* WaitAckMapIter_next(WaitAckMapIter* this)
{
   return (WaitAck*)PointerRBTreeIter_next( (RBTreeIter*)this);
}

char* WaitAckMapIter_key(WaitAckMapIter* this)
{
   return (char*)PointerRBTreeIter_key( (RBTreeIter*)this);
}

WaitAck* WaitAckMapIter_value(WaitAckMapIter* this)
{
   return (WaitAck*)PointerRBTreeIter_value( (RBTreeIter*)this);
}

bool WaitAckMapIter_end(WaitAckMapIter* this)
{
   return PointerRBTreeIter_end( (RBTreeIter*)this);
}


#endif /* WAITACKMAPITER_H_ */
