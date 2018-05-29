#ifndef POINTERRBTREE_H_
#define POINTERRBTREE_H_

#include <common/Common.h>
#include <linux/rbtree.h>


struct RBTreeElem;
typedef struct RBTreeElem RBTreeElem;
struct RBTree;
typedef struct RBTree RBTree;


/**
 * @return: <0 if key1<key2, 0 for equal keys, >0 otherwise
 */
typedef int (*TreeElemsComparator)(const void* key1, const void* key2);


struct RBTreeIter; // forward declaration of the iterator

static inline void PointerRBTree_init(RBTree* this, TreeElemsComparator compareTreeElems);
static inline void PointerRBTree_uninit(RBTree* this);

static inline RBTreeElem* _PointerRBTree_findElem(RBTree* this, const void* searchKey);

static inline bool PointerRBTree_insert(RBTree* this, void* newKey, void* newValue);
static bool PointerRBTree_erase(RBTree* this, const void* eraseKey);
static inline size_t PointerRBTree_length(RBTree* this);

static inline void PointerRBTree_clear(RBTree* this);

static inline int PointerRBTree_keyCompare(const void* a, const void* b);

extern struct RBTreeIter PointerRBTree_find(RBTree* this, const void* searchKey);
extern struct RBTreeIter PointerRBTree_begin(RBTree* this);

struct RBTreeElem
{
   struct rb_node treenode;

   void* key;
   void* value;
};


struct RBTree
{
   struct rb_root treeroot;

   size_t length;

   TreeElemsComparator compareTreeElems;
};


void PointerRBTree_init(RBTree* this, TreeElemsComparator compareTreeElems)
{
   this->treeroot.rb_node = NULL;
   this->length = 0;

   this->compareTreeElems = compareTreeElems;
}

void PointerRBTree_uninit(RBTree* this)
{
   PointerRBTree_clear(this);
}


/**
 * @return NULL if the element was not found
 */
RBTreeElem* _PointerRBTree_findElem(RBTree* this, const void* searchKey)
{
   int compRes;

   struct rb_node* node = this->treeroot.rb_node;

   while(node)
   {
      RBTreeElem* treeElem = container_of(node, RBTreeElem, treenode);

      compRes = this->compareTreeElems(searchKey, treeElem->key);

      if(compRes < 0)
         node = node->rb_left;
      else
      if(compRes > 0)
         node = node->rb_right;
      else
      { // element found => return it
         return treeElem;
      }
   }

   // element not found

   return NULL;
}


/**
 * @return true if element was inserted, false if it already existed (in which case
 * nothing is changed) or if out of mem.
 */
bool PointerRBTree_insert(RBTree* this, void* newKey, void* newValue)
{
   RBTreeElem* newElem;

   // the parent's (left or right) link to which the child will be connected
   struct rb_node** link = &this->treeroot.rb_node;
   // the parent of the new node
   struct rb_node* parent = NULL;

   while(*link)
   {
      int compRes;
      RBTreeElem* treeElem;

      parent = *link;
      treeElem = container_of(parent, RBTreeElem, treenode);

      compRes = this->compareTreeElems(newKey, treeElem->key);

      if(compRes < 0)
         link = &(*link)->rb_left;
      else
      if(compRes > 0)
         link = &(*link)->rb_right;
      else
      { // target already exists => do nothing (according to the behavior of c++ map::insert)
         //rbReplaceNode(parent, newElem, this);

         return false;
      }
   }

   // create new element

   newElem = os_kmalloc(sizeof(*newElem) );
   newElem->key = newKey;
   newElem->value = newValue;

   // link the new element
   rb_link_node(&newElem->treenode, parent, link);
   rb_insert_color(&newElem->treenode, &this->treeroot);

   this->length++;

   return true;
}

/**
 * @return false if no element with the given key existed
 */
bool PointerRBTree_erase(RBTree* this, const void* eraseKey)
{
   RBTreeElem* treeElem;

   treeElem = _PointerRBTree_findElem(this, eraseKey);
   if(!treeElem)
   { // element not found
      return false;
   }

   // unlink the element
   rb_erase(&treeElem->treenode, &this->treeroot);

   kfree(treeElem);

   this->length--;

   return true;
}

size_t PointerRBTree_length(RBTree* this)
{
   return this->length;
}

void PointerRBTree_clear(RBTree* this)
{
   while(this->length)
   {
      RBTreeElem* root = container_of(this->treeroot.rb_node, RBTreeElem, treenode);
      PointerRBTree_erase(this, root->key);
   }
}


int PointerRBTree_keyCompare(const void* a, const void* b)
{
   if (a < b)
      return -1;
   if (a == b)
      return 0;
   return 1;
}

#endif /*POINTERRBTREE_H_*/
