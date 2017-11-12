#ifndef TARGETNODEMAP_H_
#define TARGETNODEMAP_H_

#include <common/nodes/NumNodeID.h>

#include "PointerRBTree.h"
#include "PointerRBTreeIter.h"


/**
 * We assign the integer keys directly to the key pointers (i.e. no extra allocation involved,
 * but a lot of casting to convince gcc that we know what we're doing).
 * Same for the NumNodeID values.
 */


struct TargetNodeMapElem;
typedef struct TargetNodeMapElem TargetNodeMapElem;
struct TargetNodeMap;
typedef struct TargetNodeMap TargetNodeMap;

struct TargetNodeMapIter; // forward declaration of the iterator

static inline void TargetNodeMap_init(TargetNodeMap* this);
static inline void TargetNodeMap_uninit(TargetNodeMap* this);

static inline bool TargetNodeMap_insert(TargetNodeMap* this, uint16_t newKey, NumNodeID newValue);
static inline bool TargetNodeMap_erase(TargetNodeMap* this, const uint16_t eraseKey);
static inline size_t TargetNodeMap_length(TargetNodeMap* this);

static inline void TargetNodeMap_clear(TargetNodeMap* this);

extern struct TargetNodeMapIter TargetNodeMap_find(TargetNodeMap* this, const uint16_t searchKey);
extern struct TargetNodeMapIter TargetNodeMap_begin(TargetNodeMap* this);


struct TargetNodeMapElem
{
   RBTreeElem rbTreeElem;
};

/**
 * keys: uint16_t; values: NumNodeID
 */
struct TargetNodeMap
{
   RBTree rbTree;
};


void TargetNodeMap_init(TargetNodeMap* this)
{
   PointerRBTree_init( (RBTree*)this, PointerRBTree_keyCompare);
}

void TargetNodeMap_uninit(TargetNodeMap* this)
{
   // (nothing alloc'ed by this sub map type => nothing special here to be free'd)

   PointerRBTree_uninit( (RBTree*)this);
}


/**
 * @return true if element was inserted, false if key already existed (in which case
 * nothing is changed)
 */
bool TargetNodeMap_insert(TargetNodeMap* this, uint16_t newKey, NumNodeID newValue)
{
   return PointerRBTree_insert((RBTree*) this, (void*)(size_t)newKey,
      (void*)(size_t) newValue.value);
}

/**
 * @return false if no element with the given key existed
 */
bool TargetNodeMap_erase(TargetNodeMap* this, const uint16_t eraseKey)
{
   return PointerRBTree_erase( (RBTree*)this, (const void*)(const size_t)eraseKey);
}

size_t TargetNodeMap_length(TargetNodeMap* this)
{
   return PointerRBTree_length( (RBTree*)this);
}


void TargetNodeMap_clear(TargetNodeMap* this)
{
   PointerRBTree_clear(&this->rbTree);
}

#endif /* TARGETNODEMAP_H_ */
