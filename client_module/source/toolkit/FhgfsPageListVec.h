#ifndef FHGFSPAGELISTVEC_H_
#define FHGFSPAGELISTVEC_H_

#include <linux/mempool.h>

#include "FhgfsPage.h"

// max pages in a single pageVec, but by using pageVecList's many of these may be used
#ifdef BEEGFS_DEBUG
#define BEEGFS_LIST_VEC_MAX_PAGES    16 // if debugging is enable we make sure lists are tested
#else
#define BEEGFS_LIST_VEC_MAX_PAGES    32
#endif

struct FhgfsPageListVec;
typedef struct FhgfsPageListVec FhgfsPageListVec;


static inline FhgfsPageListVec* FhgfsPageListVec_create(struct kmem_cache* pageVecCache,
   mempool_t* pageListPool, bool* outIsPoolAlloc);
static inline void FhgfsPageListVec_destroy(FhgfsPageListVec* this,
   struct kmem_cache* pageVecCache, mempool_t* pageVecPool);
static inline void FhgfsPageListVec_init(FhgfsPageListVec* this);
static inline void FhgfsPageListVec_uninit(FhgfsPageListVec* this);

static inline bool FhgfsPageListVec_pushPage(FhgfsPageListVec* this, struct page* page,
   int usedPageLength);

static inline size_t FhgfsPageListVec_getMaxPages(FhgfsPageListVec* this);

static inline FhgfsPage* FhgfsPageListVec_getFhgfsPage(FhgfsPageListVec* this, size_t index);



struct FhgfsPageListVec
{
   FhgfsPage fhgfsPages[BEEGFS_LIST_VEC_MAX_PAGES];
   size_t usedPages; // number of used pages of this vector

   struct list_head list;
};


/**
 * Constructor
 *
 * @param pageVecPool is only set if this is the first allocation from FhgfsChunkPageVec, as
 *    this allocation must not fail.
 * @param outIsPoolAlloc is supposed to be initialized to false
 */
FhgfsPageListVec* FhgfsPageListVec_create(struct kmem_cache* pageVecCache,
   mempool_t* pageVecPool, bool* outIsPoolAlloc)
{
   FhgfsPageListVec* this;

#ifdef BEEGFS_DEBUG
   // test cache alloc failures in debug mode
   if (jiffies & 1) // odd jiffies simulate an allocation error
      this = NULL;
   else
      this = (FhgfsPageListVec*) kmem_cache_alloc(pageVecCache, GFP_NOFS);
#else
   this = (FhgfsPageListVec*) kmem_cache_alloc(pageVecCache, GFP_NOFS);
#endif

   if (unlikely(!this) )
   {
      if (pageVecPool)
      {
         this = mempool_alloc(pageVecPool, GFP_NOFS);
         if (unlikely(!this) )
         {
            printk_fhgfs(KERN_WARNING, "%s: mempool_alloc unexpectedly failed\n", __func__);
            return NULL;
         }

         *outIsPoolAlloc = true;
      }
      else
         return NULL;
   }

   FhgfsPageListVec_init(this);

   return this;
}

/**
 * Destructor
 */
void FhgfsPageListVec_destroy(FhgfsPageListVec* this, struct kmem_cache* pageVecCache,
   mempool_t* pageVecPool)
{
   FhgfsPageListVec_uninit(this);

   if (pageVecPool)
      mempool_free(this, pageVecPool);
   else
      kmem_cache_free(pageVecCache, this);
}


/**
 * Initialize FhgfsPageListVec
 */
void FhgfsPageListVec_init(FhgfsPageListVec* this)
{
   this->usedPages = 0;
   INIT_LIST_HEAD(&this->list);
}

/**
 * Uninitialize FhgfsPageListVec
 */
void FhgfsPageListVec_uninit(FhgfsPageListVec* this)
{
   // no-op
}


/**
 * Add (append) a kernel page to this pageListVec
 *
 * @return   false if the pageListVec is full, true if adding succeeded
 */
bool FhgfsPageListVec_pushPage(FhgfsPageListVec* this, struct page* page, int usedPageLength)
{
   FhgfsPage* fhgfsPage;

   if (this->usedPages == BEEGFS_LIST_VEC_MAX_PAGES)
      return false;

   fhgfsPage = &this->fhgfsPages[this->usedPages];
   this->usedPages++;

   fhgfsPage->page   = page;
   fhgfsPage->data   = kmap(page);
   fhgfsPage->length = usedPageLength;

   return true;
}

/**
 * Return the number of max pages of this vector
 */
size_t FhgfsPageListVec_getMaxPages(FhgfsPageListVec* this)
{
   return BEEGFS_LIST_VEC_MAX_PAGES;
}

/**
 * Return the page at the given index
 */
FhgfsPage* FhgfsPageListVec_getFhgfsPage(FhgfsPageListVec* this, size_t index)
{
   return &this->fhgfsPages[index];
}

#endif // FHGFSPAGELISTVEC_H_
