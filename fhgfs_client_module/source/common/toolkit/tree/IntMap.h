#ifndef INTMAP_H_
#define INTMAP_H_

#include "PointerRBTree.h"
#include "PointerRBTreeIter.h"


/**
 * We assign the integer keys directly to the key pointers (i.e. no extra allocation involved,
 * but a lot of casting to convince gcc that we know what we're doing).
 * Values are just arbitrary pointers.
 */


struct IntMapElem;
typedef struct IntMapElem IntMapElem;
struct IntMap;
typedef struct IntMap IntMap;

struct IntMapIter; // forward declaration of the iterator

static inline void IntMap_init(IntMap* this);
static inline void IntMap_uninit(IntMap* this);

static inline bool IntMap_insert(IntMap* this, int newKey, char* newValue);
static inline bool IntMap_erase(IntMap* this, const int eraseKey);
static inline size_t IntMap_length(IntMap* this);

static inline void IntMap_clear(IntMap* this);

extern struct IntMapIter IntMap_find(IntMap* this, const int searchKey);
extern struct IntMapIter IntMap_begin(IntMap* this);


struct IntMapElem
{
   RBTreeElem rbTreeElem;
};

struct IntMap
{
   RBTree rbTree;
};


void IntMap_init(IntMap* this)
{
   PointerRBTree_init( (RBTree*)this, PointerRBTree_keyCompare);
}

void IntMap_uninit(IntMap* this)
{
   // (nothing alloc'ed by this sub map type => nothing special here to be free'd)

   PointerRBTree_uninit( (RBTree*)this);
}


/**
 * @param newValue just assigned, not copied.
 * @return true if element was inserted, false if key already existed (in which case
 * nothing is changed)
 */
bool IntMap_insert(IntMap* this, int newKey, char* newValue)
{
   return PointerRBTree_insert( (RBTree*)this, (void*)(size_t)newKey, newValue);
}

/**
 * @return false if no element with the given key existed
 */
bool IntMap_erase(IntMap* this, const int eraseKey)
{
   return PointerRBTree_erase( (RBTree*)this, (const void*)(const size_t)eraseKey);
}

size_t IntMap_length(IntMap* this)
{
   return PointerRBTree_length( (RBTree*)this);
}


void IntMap_clear(IntMap* this)
{
   PointerRBTree_clear(&this->rbTree);
}

#endif /* INTMAP_H_ */
