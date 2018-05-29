#ifndef MIRRORBUDDYGROUPMAPITER_H_
#define MIRRORBUDDYGROUPMAPITER_H_

#include "MirrorBuddyGroupMap.h"

struct MirrorBuddyGroupMapIter;
typedef struct MirrorBuddyGroupMapIter MirrorBuddyGroupMapIter;

static inline void MirrorBuddyGroupMapIter_init(MirrorBuddyGroupMapIter* this,
   MirrorBuddyGroupMap* map, RBTreeElem* treeElem);
static inline bool MirrorBuddyGroupMapIter_hasNext(MirrorBuddyGroupMapIter* this);
static inline void MirrorBuddyGroupMapIter_next(MirrorBuddyGroupMapIter* this);
static inline uint16_t MirrorBuddyGroupMapIter_key(MirrorBuddyGroupMapIter* this);
static inline MirrorBuddyGroup* MirrorBuddyGroupMapIter_value(MirrorBuddyGroupMapIter* this);
static inline bool MirrorBuddyGroupMapIter_end(MirrorBuddyGroupMapIter* this);

struct MirrorBuddyGroupMapIter
{
   RBTreeIter rbTreeIter;
};

void MirrorBuddyGroupMapIter_init(MirrorBuddyGroupMapIter* this, MirrorBuddyGroupMap* map,
   RBTreeElem* treeElem)
{
   PointerRBTreeIter_init((RBTreeIter*) this, (RBTree*) map, (RBTreeElem*) treeElem);
}

bool MirrorBuddyGroupMapIter_hasNext(MirrorBuddyGroupMapIter* this)
{
   return PointerRBTreeIter_hasNext((RBTreeIter*) this);
}

void MirrorBuddyGroupMapIter_next(MirrorBuddyGroupMapIter* this)
{
   PointerRBTreeIter_next((RBTreeIter*) this);
}

uint16_t MirrorBuddyGroupMapIter_key(MirrorBuddyGroupMapIter* this)
{
   return (uint16_t) (size_t) PointerRBTreeIter_key((RBTreeIter*) this);
}

MirrorBuddyGroup* MirrorBuddyGroupMapIter_value(MirrorBuddyGroupMapIter* this)
{
   return (MirrorBuddyGroup*) (size_t) PointerRBTreeIter_value((RBTreeIter*) this);
}

bool MirrorBuddyGroupMapIter_end(MirrorBuddyGroupMapIter* this)
{
   return PointerRBTreeIter_end((RBTreeIter*) this);
}

#endif /* MIRRORBUDDYGROUPMAPITER_H_ */
