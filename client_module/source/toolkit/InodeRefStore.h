#ifndef INODEREFSTORE_H_
#define INODEREFSTORE_H_

#include <common/Common.h>
#include <common/toolkit/tree/PointerRBTree.h>
#include <common/toolkit/tree/PointerRBTreeIter.h>
#include <common/threading/Mutex.h>
#include <os/OsDeps.h>


/*
 * This class saves references to inodes in a tree. It has special methods to make it suitable
 * for async cache flushes.
 */

struct InodeRefStore;
typedef struct InodeRefStore InodeRefStore;

static inline void InodeRefStore_init(InodeRefStore* this);
static inline InodeRefStore* InodeRefStore_construct(void);
static inline void InodeRefStore_uninit(InodeRefStore* this);
static inline void InodeRefStore_destruct(InodeRefStore* this);

static inline void InodeRefStore_addAndReferenceInode(InodeRefStore* this, struct inode* inode);
static inline void InodeRefStore_addOrPutInode(InodeRefStore* this, struct inode* inode);
static inline struct inode* InodeRefStore_getAndRemoveFirstInode(InodeRefStore* this);
static inline struct inode* InodeRefStore_getAndRemoveNextInode(InodeRefStore* this,
   struct inode* oldInode);
static inline bool InodeRefStore_removeAndReleaseInode(InodeRefStore* this,
   struct inode* inode);

// getters & setters
static inline size_t InodeRefStore_getSize(InodeRefStore* this);


// static
extern int __InodeRefStore_keyComparator(const void* key1, const void* key2);


struct InodeRefStore
{
   RBTree tree; // keys are inode pointers, values are unused (NULL)

   Mutex mutex;
};


void InodeRefStore_init(InodeRefStore* this)
{
   Mutex_init(&this->mutex);

   PointerRBTree_init(&this->tree, __InodeRefStore_keyComparator);
}

struct InodeRefStore* InodeRefStore_construct(void)
{
   struct InodeRefStore* this = (InodeRefStore*)os_kmalloc(sizeof(*this) );

   InodeRefStore_init(this);

   return this;
}

void InodeRefStore_uninit(InodeRefStore* this)
{
   RBTreeIter iter = PointerRBTree_begin(&this->tree);
   for( ; !PointerRBTreeIter_end(&iter); PointerRBTreeIter_next(&iter) )
   {
      struct inode* inode = PointerRBTreeIter_key(&iter);
      iput(inode);
   }

   PointerRBTree_uninit(&this->tree);

   Mutex_uninit(&this->mutex);
}

void InodeRefStore_destruct(InodeRefStore* this)
{
   InodeRefStore_uninit(this);

   kfree(this);
}

/**
 * Adds the given inode to the tree if not there yet.
 *
 * Note: The inode is referenced by calling ihold(). It must later be released by calling
 * iput().
 */
void InodeRefStore_addAndReferenceInode(InodeRefStore* this, struct inode* inode)
{
   Mutex_lock(&this->mutex);

   // only insert it, if not already within the three (mutex lock required)
   if(PointerRBTree_insert(&this->tree, inode, NULL) )
   {
      /* Only increase the counter once we know we had to insert it.
       * Also increase the counter within the lock to avoid racing with _removeAndRelease() */
      ihold(inode);
   }

   Mutex_unlock(&this->mutex);
}

/**
 * Adds the given inode to the tree or drops the reference if the inode already exists in the tree.
 *
 * Note: Usually, _addAndReferenceInode() should be used; this is only called if caller ensured
 * that there is an extra inode reference (igrab() ) for this inode.
 */
void InodeRefStore_addOrPutInode(InodeRefStore* this, struct inode* inode)
{
   Mutex_lock(&this->mutex);

   // only insert it, if not already within the three (mutex lock required)
   if(!PointerRBTree_insert(&this->tree, inode, NULL) )
   {
      /* Decrease ref counter if the inode already exists, because that means the caller who
       * added the inode already increased the ref count - and we only want to have one single
       * reference per inode in this store.
       * Also decrease the counter within the lock to avoid racing with _removeAndRelease() */
      iput(inode);
   }

   Mutex_unlock(&this->mutex);
}

/**
 * Get first element in the store (for iteration).
 *
 * Note: If the inode was added by _addAndReferenceInode(), the caller will probably also want to
 * call iput() after work with the returned inode is complete.
 *
 * @return NULL if the store is empty
 */
struct inode* InodeRefStore_getAndRemoveFirstInode(InodeRefStore* this)
{
   struct inode* firstInode = NULL;
   RBTreeIter iter;

   Mutex_lock(&this->mutex);

   iter = PointerRBTree_begin(&this->tree);
   if(!PointerRBTreeIter_end(&iter) )
   {
      firstInode = PointerRBTreeIter_key(&iter);

      PointerRBTree_erase(&this->tree, firstInode);
   }

   Mutex_unlock(&this->mutex);

   return firstInode;
}

/**
 * Get next inode and remove it from the store. Typically used for iteration based on
 * _getAndRemoveFirstInode().
 *
 * Note: If the inode was added by _addAndReferenceInode(), the caller will probably also want to
 * call iput() after work with the returned inode is complete.
 *
 * @return NULL if no next inode exists in the store
 */
struct inode* InodeRefStore_getAndRemoveNextInode(InodeRefStore* this, struct inode* oldInode)
{
   struct inode* nextInode = NULL;
   RBTreeIter iter;

   Mutex_lock(&this->mutex);

   iter = PointerRBTree_find(&this->tree, oldInode);
   if(!PointerRBTreeIter_end(&iter) )
   { // fast path: oldInode still (or again) exists in the tree, so we can easily find next

      // retrieve (and remove) next element
      PointerRBTreeIter_next(&iter);
      if(!PointerRBTreeIter_end(&iter) )
      {
         nextInode = PointerRBTreeIter_key(&iter);

         PointerRBTree_erase(&this->tree, nextInode);
      }
   }
   else
   { // slow path: oldInode doesn't exist, so we need to insert it to find the next element

      // temporarily insert oldInode to find next element
      PointerRBTree_insert(&this->tree, oldInode, NULL);
      iter = PointerRBTree_find(&this->tree, oldInode);

      // retrieve (and remove) next element
      PointerRBTreeIter_next(&iter);
      if(!PointerRBTreeIter_end(&iter) )
      {
         nextInode = PointerRBTreeIter_key(&iter);

         PointerRBTree_erase(&this->tree, nextInode);
      }

      // remove temporarily inserted oldInode
      PointerRBTree_erase(&this->tree, oldInode);
   }

   Mutex_unlock(&this->mutex);

   return nextInode;
}

/**
 * Remove inode from store. Only if it existed in the store, we also drop an inode reference.
 *
 * @return false if no such element existed, true otherwise
 */
bool InodeRefStore_removeAndReleaseInode(InodeRefStore* this, struct inode* inode)
{
   bool eraseRes;

   Mutex_lock(&this->mutex);

   eraseRes = PointerRBTree_erase(&this->tree, inode);
   if(eraseRes)
      iput(inode); // iput must be inside the mutex to avoid races with _addAndReference()

   Mutex_unlock(&this->mutex);

   return eraseRes;
}


size_t InodeRefStore_getSize(InodeRefStore* this)
{
   size_t retVal;

   Mutex_lock(&this->mutex);

   retVal = PointerRBTree_length(&this->tree);

   Mutex_unlock(&this->mutex);

   return retVal;
}


#endif /* INODEREFSTORE_H_ */
