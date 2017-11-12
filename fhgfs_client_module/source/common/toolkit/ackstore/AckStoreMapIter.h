#ifndef ACKSTOREMAPITER_H_
#define ACKSTOREMAPITER_H_

#include "AckStoreMap.h"

struct AckStoreMapIter;
typedef struct AckStoreMapIter AckStoreMapIter;

static inline void AckStoreMapIter_init(AckStoreMapIter* this, AckStoreMap* map, RBTreeElem* treeElem);
static inline bool AckStoreMapIter_hasNext(AckStoreMapIter* this);
static inline AckStoreEntry* AckStoreMapIter_next(AckStoreMapIter* this);
static inline char* AckStoreMapIter_key(AckStoreMapIter* this);
static inline AckStoreEntry* AckStoreMapIter_value(AckStoreMapIter* this);
static inline bool AckStoreMapIter_end(AckStoreMapIter* this);

struct AckStoreMapIter
{
   RBTreeIter rbTreeIter;
};

void AckStoreMapIter_init(AckStoreMapIter* this, AckStoreMap* map, RBTreeElem* treeElem)
{
   PointerRBTreeIter_init( (RBTreeIter*)this, (RBTree*)map, (RBTreeElem*)treeElem);
}

bool AckStoreMapIter_hasNext(AckStoreMapIter* this)
{
   return PointerRBTreeIter_hasNext( (RBTreeIter*)this);
}

AckStoreEntry* AckStoreMapIter_next(AckStoreMapIter* this)
{
   return (AckStoreEntry*)PointerRBTreeIter_next( (RBTreeIter*)this);
}

char* AckStoreMapIter_key(AckStoreMapIter* this)
{
   return (char*)PointerRBTreeIter_key( (RBTreeIter*)this);
}

AckStoreEntry* AckStoreMapIter_value(AckStoreMapIter* this)
{
   return (AckStoreEntry*)PointerRBTreeIter_value( (RBTreeIter*)this);
}

bool AckStoreMapIter_end(AckStoreMapIter* this)
{
   return PointerRBTreeIter_end( (RBTreeIter*)this);
}


#endif /* ACKSTOREMAPITER_H_ */
