#ifndef MIRRORBUDDYGROUPMAP_H_
#define MIRRORBUDDYGROUPMAP_H_


#include <common/nodes/MirrorBuddyGroup.h>
#include "PointerRBTree.h"
#include "PointerRBTreeIter.h"

/**
 * We assign the uint16 keys directly to the key pointers (i.e. no extra allocation
 * involved, but a lot of casting to convince gcc that we know what we're doing).
 * Values are MirrorBuddyGroup pointers.
 */


struct MirrorBuddyGroupMapElem;
typedef struct MirrorBuddyGroupMapElem MirrorBuddyGroupMapElem;
struct MirrorBuddyGroupMap;
typedef struct MirrorBuddyGroupMap MirrorBuddyGroupMap;

struct MirrorBuddyGroupMapIter; // forward declaration of the iterator

static inline void MirrorBuddyGroupMap_init(MirrorBuddyGroupMap* this);
static inline MirrorBuddyGroupMap* MirrorBuddyGroupMap_construct(void);
static inline void MirrorBuddyGroupMap_uninit(MirrorBuddyGroupMap* this);
static inline void MirrorBuddyGroupMap_destruct(MirrorBuddyGroupMap* this);

static inline bool MirrorBuddyGroupMap_insert(MirrorBuddyGroupMap* this, uint16_t newKey,
   MirrorBuddyGroup* newValue);
static inline bool MirrorBuddyGroupMap_erase(MirrorBuddyGroupMap* this,
   const uint16_t eraseKey);
static inline size_t MirrorBuddyGroupMap_length(MirrorBuddyGroupMap* this);

static inline void MirrorBuddyGroupMap_clear(MirrorBuddyGroupMap* this);

extern struct MirrorBuddyGroupMapIter MirrorBuddyGroupMap_find(MirrorBuddyGroupMap* this,
   const uint16_t searchKey);
extern struct MirrorBuddyGroupMapIter MirrorBuddyGroupMap_begin(MirrorBuddyGroupMap* this);


struct MirrorBuddyGroupMapElem
{
   RBTreeElem rbTreeElem;
};

struct MirrorBuddyGroupMap
{
   RBTree rbTree;
};


void MirrorBuddyGroupMap_init(MirrorBuddyGroupMap* this)
{
   PointerRBTree_init( (RBTree*)this, PointerRBTree_keyCompare);
}

MirrorBuddyGroupMap* MirrorBuddyGroupMap_construct(void)
{
   MirrorBuddyGroupMap* this = (MirrorBuddyGroupMap*)os_kmalloc(sizeof(*this) );

   MirrorBuddyGroupMap_init(this);

   return this;
}

void MirrorBuddyGroupMap_uninit(MirrorBuddyGroupMap* this)
{
   PointerRBTree_uninit( (RBTree*)this);
}

void MirrorBuddyGroupMap_destruct(MirrorBuddyGroupMap* this)
{
   MirrorBuddyGroupMap_uninit(this);

   kfree(this);
}


/**
 * @param newValue just assigned, not copied.
 * @return true if element was inserted, false if key already existed (in which case
 * nothing is changed)
 */
bool MirrorBuddyGroupMap_insert(MirrorBuddyGroupMap* this, uint16_t newKey,
   MirrorBuddyGroup* newValue)
{
   return PointerRBTree_insert( (RBTree*)this, (void*)(size_t)newKey, newValue);
}

/**
 * @return false if no element with the given key existed
 */
bool MirrorBuddyGroupMap_erase(MirrorBuddyGroupMap* this, const uint16_t eraseKey)
{
   return PointerRBTree_erase( (RBTree*)this, (const void*)(const size_t)eraseKey);
}

size_t MirrorBuddyGroupMap_length(MirrorBuddyGroupMap* this)
{
   return PointerRBTree_length( (RBTree*)this);
}


void MirrorBuddyGroupMap_clear(MirrorBuddyGroupMap* this)
{
   PointerRBTree_clear(&this->rbTree);
}

#endif /* MIRRORBUDDYGROUPMAP_H_ */
