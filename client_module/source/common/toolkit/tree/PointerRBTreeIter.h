#ifndef POINTERRBTREEITER_H_
#define POINTERRBTREEITER_H_

#include "PointerRBTree.h"

struct RBTreeIter;
typedef struct RBTreeIter RBTreeIter;

static inline void PointerRBTreeIter_init(
   RBTreeIter* this, RBTree* tree, RBTreeElem* treeElem);

static inline void* PointerRBTreeIter_next(RBTreeIter* this);
static inline void* PointerRBTreeIter_key(RBTreeIter* this);
static inline void* PointerRBTreeIter_value(RBTreeIter* this);
static inline bool PointerRBTreeIter_end(RBTreeIter* this);

struct RBTreeIter
{
   RBTree* tree;
   RBTreeElem* treeElem;
};


void PointerRBTreeIter_init(RBTreeIter* this, RBTree* tree, RBTreeElem* treeElem)
{
   this->tree = tree;
   this->treeElem = treeElem;
}

void* PointerRBTreeIter_next(RBTreeIter* this)
{
   struct rb_node* next = rb_next(&this->treeElem->treenode);

   this->treeElem = next ? container_of(next, RBTreeElem, treenode) : NULL;

   return this->treeElem;
}

void* PointerRBTreeIter_key(RBTreeIter* this)
{
   return this->treeElem->key;
}

void* PointerRBTreeIter_value(RBTreeIter* this)
{
   return this->treeElem->value;
}

/**
 * Return true if the end of the iterator was reached
 */
bool PointerRBTreeIter_end(RBTreeIter* this)
{
   return (this->treeElem == NULL);
}

#endif /*POINTERRBTREEITER_H_*/
