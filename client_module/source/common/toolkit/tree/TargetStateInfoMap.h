#ifndef TARGETSTATEINFOMAP_H_
#define TARGETSTATEINFOMAP_H_


#include <common/nodes/TargetStateInfo.h>
#include "PointerRBTree.h"
#include "PointerRBTreeIter.h"

/**
 * We assign the uint16 keys directly to the key pointers (i.e. no extra allocation
 * involved, but a lot of casting to convince gcc that we know what we're doing).
 * Values are TargetStateInfo pointers.
 */

struct TargetStateInfoMapElem;
typedef struct TargetStateInfoMapElem TargetStateInfoMapElem;
struct TargetStateInfoMap;
typedef struct TargetStateInfoMap TargetStateInfoMap;

struct TargetStateInfoMapIter; // forward declaration of the iterator

static inline void TargetStateInfoMap_init(TargetStateInfoMap* this);
static inline void TargetStateInfoMap_uninit(TargetStateInfoMap* this);

static inline bool TargetStateInfoMap_insert(TargetStateInfoMap* this, uint16_t newKey,
   TargetStateInfo* newValue);
static inline bool TargetStateInfoMap_erase(TargetStateInfoMap* this,
   const uint16_t eraseKey);
static inline size_t TargetStateInfoMap_length(TargetStateInfoMap* this);

static inline void TargetStateInfoMap_clear(TargetStateInfoMap* this);

extern struct TargetStateInfoMapIter TargetStateInfoMap_find(TargetStateInfoMap* this,
   const uint16_t searchKey);
extern struct TargetStateInfoMapIter TargetStateInfoMap_begin(TargetStateInfoMap* this);


struct TargetStateInfoMapElem
{
   RBTreeElem rbTreeElem;
};

struct TargetStateInfoMap
{
   RBTree rbTree;
};


void TargetStateInfoMap_init(TargetStateInfoMap* this)
{
   PointerRBTree_init( (RBTree*)this, PointerRBTree_keyCompare);
}

void TargetStateInfoMap_uninit(TargetStateInfoMap* this)
{
   PointerRBTree_uninit( (RBTree*)this);
}


/**
 * @param newValue just assigned, not copied.
 * @return true if element was inserted, false if key already existed (in which case
 * nothing is changed)
 */
bool TargetStateInfoMap_insert(TargetStateInfoMap* this, uint16_t newKey,
   TargetStateInfo* newValue)
{
   return PointerRBTree_insert( (RBTree*)this, (void*)(size_t)newKey, newValue);
}

/**
 * @return false if no element with the given key existed
 */
bool TargetStateInfoMap_erase(TargetStateInfoMap* this, const uint16_t eraseKey)
{
   return PointerRBTree_erase( (RBTree*)this, (const void*)(const size_t)eraseKey);
}

size_t TargetStateInfoMap_length(TargetStateInfoMap* this)
{
   return PointerRBTree_length( (RBTree*)this);
}


void TargetStateInfoMap_clear(TargetStateInfoMap* this)
{
   PointerRBTree_clear(&this->rbTree);
}

#endif /* TARGETSTATEINFOMAP_H_ */
