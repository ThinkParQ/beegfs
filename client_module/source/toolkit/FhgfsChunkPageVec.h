#ifndef FHGFSCHUNKPAGEVEC_H_
#define FHGFSCHUNKPAGEVEC_H_

#include <linux/types.h>
#include <linux/fs.h>

#include <os/OsCompat.h>
#include "FhgfsPage.h"
#include "FhgfsPageListVec.h"

#include <linux/writeback.h> // struct write_back control



struct FhgfsChunkPageVec;
typedef struct FhgfsChunkPageVec FhgfsChunkPageVec;


static inline FhgfsChunkPageVec* FhgfsChunkPageVec_create(App *app, struct inode* inode,
   struct kmem_cache* pageVecCache, mempool_t* pageVecPool, unsigned numChunkPages);
static inline void FhgfsChunkPageVec_destroy(FhgfsChunkPageVec* this);


static inline int FhgfsChunkPageVec_init(FhgfsChunkPageVec* this, App *app, struct inode* inode,
    struct kmem_cache* pageVecCache, mempool_t* pageVecPool, unsigned numChunkPages);
static inline  void FhgfsChunkPageVec_uninit(FhgfsChunkPageVec* this);

static inline int FhgfsChunkPageVec_addPageListVec(FhgfsChunkPageVec* this, mempool_t* pageVecPool);
static inline FhgfsPageListVec* FhgfsChunkPageVec_getFirstPageListVec(FhgfsChunkPageVec* this);


static inline  bool FhgfsChunkPageVec_pushPage(FhgfsChunkPageVec* this, struct page* page,
   int usedPageLength);

static inline unsigned FhgfsChunkPageVec_getSize(FhgfsChunkPageVec* this);

static inline struct inode* FhgfsChunkPageVec_getInode(FhgfsChunkPageVec* this);

static inline loff_t FhgfsChunkPageVec_getFirstPageFileOffset(FhgfsChunkPageVec* this);

static inline FhgfsPage* FhgfsChunkPageVec_iterateGetNextPage(FhgfsChunkPageVec* this);
static inline void FhgfsChunkPageVec_resetIterator(FhgfsChunkPageVec* this);

static inline size_t FhgfsChunkPageVec_getCurrentListVecIterIdx(FhgfsChunkPageVec* this);

static inline size_t FhgfsChunkPageVec_getCurrentListVecIterIdx(FhgfsChunkPageVec* this);

static inline size_t FhgfsChunkPageVec_getDataSize(FhgfsChunkPageVec* this);
static inline size_t FhgfsChunkPageVec_getRemainingDataSize(FhgfsChunkPageVec* this);

static inline unsigned _FhgfsChunkPageVec_getChunkPageOffset(FhgfsChunkPageVec* this,
   pgoff_t index);

extern void FhgfsChunkPageVec_iterateAllHandleWritePages(FhgfsChunkPageVec* this, int writeRes);
extern void FhgfsChunkPageVec_iterateAllHandleReadErr(FhgfsChunkPageVec* this);



struct FhgfsChunkPageVec
{
   App* app;
   struct inode* inode;
   struct kmem_cache* pageListVecCache;
   mempool_t* pageVecPool; // only set to non-NULL if there was a pageVec pool allocation

   unsigned numChunkPages;

   unsigned size;    // number of used elements of the vector */

   struct list_head pageListVecHead;   // list of FhgfsPageListVec entries
   FhgfsPageListVec* lastAllocListVec; /* last list entry that was allocated, new pages will be
                                        * added to this vector */
   FhgfsPageListVec* currentIterListVec; /* On iterating over the list vector this is the
                                          * current FhgfsPageListVec element */
   size_t currentVecIdx;                 // current vector index within currentIterListVec
   size_t maxPagesPerVec;                // number of vector pages per FhgfsPageListVec
   size_t currentListVecIterIdx;         /* index as if the current page would be in a large single
                                          * array. Note: For log and debugging only */

   size_t lastPageUsedDataSize; // used bytes of the last page


#ifdef BEEGFS_DEBUG
   int numListVecs;
#endif

   loff_t firstPageFileOffset;
   unsigned firstPageChunkOffset;
   pgoff_t firstPageIdx;
   pgoff_t lastPageIdx;
};


/**
 * Constructor
 */
FhgfsChunkPageVec* FhgfsChunkPageVec_create(App* app, struct inode* inode,
   struct kmem_cache* pageVecCache, mempool_t* pageVecPool, unsigned numChunkPages)
{
   int initErr;

    FhgfsChunkPageVec* this = os_kmalloc(sizeof(*this) );
    if (unlikely(!this) )
       return NULL;

    initErr = FhgfsChunkPageVec_init(this, app, inode, pageVecCache, pageVecPool, numChunkPages);
    if (unlikely(initErr) )
    {
       kfree(this);
       return NULL;
    }

    return this;
}


/**
 * Destructor
 */
void FhgfsChunkPageVec_destroy(FhgfsChunkPageVec* this)
{
   FhgfsChunkPageVec_uninit(this);

   kfree(this);
}



/**
 * Initialize FhgfsChunkPageVec
 *
 * @return 0 on success, negative linux error code on error
 */
int FhgfsChunkPageVec_init(FhgfsChunkPageVec* this, App* app, struct inode* inode,
   struct kmem_cache* pageVecCache, mempool_t* pageVecPool, unsigned numChunkPages)
{
   int listVecAddRes;

   this->app     = app;
   this->inode   = inode;
   this->pageListVecCache = pageVecCache;
   this->pageVecPool = NULL; // first initialize to NULL, it will be set later if required

   this->numChunkPages = numChunkPages;
   this->size    = 0;
   this->firstPageChunkOffset = 0;

   INIT_LIST_HEAD(&this->pageListVecHead);

#ifdef BEEGFS_DEBUG
   this->numListVecs = 0;
#endif

   // add the first pageListVec
   listVecAddRes = FhgfsChunkPageVec_addPageListVec(this, pageVecPool);
   if (unlikely(listVecAddRes) )
      return listVecAddRes;

   return 0;
}


/**
 * Uninitialize the vector
 */
void FhgfsChunkPageVec_uninit(FhgfsChunkPageVec* this)
{
   while (!list_empty(&this->pageListVecHead) )
   {
      FhgfsPageListVec* listVec = list_entry(this->pageListVecHead.prev, FhgfsPageListVec, list);

      list_del(&listVec->list);

      FhgfsPageListVec_destroy(listVec, this->pageListVecCache, this->pageVecPool);

#ifdef BEEGFS_DEBUG
      this->numListVecs--;
#endif

   }

#ifdef BEEGFS_DEBUG
   if (unlikely(this->numListVecs) )
   {
      Logger* log = App_getLogger(this->app);

      Logger_logErrFormatted(log, __func__, "Bug: Failed to distroy all listVecs, remaining: %d",
         this->numListVecs);
   }
#endif

}



/**
 * Create a new pageListVec entry and add it to the list
 *
 * @param pageVecPool must be set from FhgfsChunkPageVec_init, but is NULL on adding
 *    additional pageListVecs (secondary allocation)
 */
int FhgfsChunkPageVec_addPageListVec(FhgfsChunkPageVec* this, mempool_t* pageVecPool)
{
   bool isPoolAlloc = false;

   FhgfsPageListVec* pageListVec = FhgfsPageListVec_create(this->pageListVecCache, pageVecPool,
      &isPoolAlloc);
   if (unlikely(!pageListVec) )
         return -ENOMEM;

   if (isPoolAlloc)
      this->pageVecPool = pageVecPool;

   if (list_empty(&this->pageListVecHead) )
   {  // first entry
      this->currentIterListVec = pageListVec;
      this->currentVecIdx = 0;
      this->currentListVecIterIdx = 0;
      this->maxPagesPerVec = FhgfsPageListVec_getMaxPages(pageListVec);
   }

   // add this listVec head to the list
   list_add_tail(&pageListVec->list, &this->pageListVecHead);

   this->lastAllocListVec = pageListVec;

#ifdef BEEGFS_DEBUG
   this->numListVecs++;
#endif

   return 0;
}


/*
 * Get the first pageListVec
 */
FhgfsPageListVec* FhgfsChunkPageVec_getFirstPageListVec(FhgfsChunkPageVec* this)
{
   if (list_empty(&this->pageListVecHead) )
      return NULL;

   return list_first_entry(&this->pageListVecHead, FhgfsPageListVec, list);
}


/**
 * Append another page to the page list-vector
 *
 * Note: Returns false if the page-vec is full or a non-consecutive page is to be added
 */
bool FhgfsChunkPageVec_pushPage(FhgfsChunkPageVec* this, struct page* page,
   int usedPageLength)
{
   const char* logContext = __func__;
   bool pushSuccess;

   if ((this->size  + this->firstPageChunkOffset) == this->numChunkPages)
      return false; // pageVec full

   if ((this->size) && (this->lastPageIdx + 1 != page->index) )
      return false; // non-consecutive page

   pushSuccess = FhgfsPageListVec_pushPage(this->lastAllocListVec, page,
      usedPageLength);

   if (!pushSuccess)
   {
      /* NOTE: pageVecPool must be set to NULL in _addPageListVec here, to ensure the pool is not
       *       used multiple times (2nd alloc might wait for the 1st one to complete and so might
       *        never succeed). */
      int listVecAddRes = FhgfsChunkPageVec_addPageListVec(this, NULL);
      if (listVecAddRes)
         return false;

      // now try again, should work now, if not we give up
      pushSuccess = FhgfsPageListVec_pushPage(this->lastAllocListVec, page, usedPageLength);
      if (!pushSuccess)
      {
         Logger* log = App_getLogger(this->app);

         Logger_logErr(log, logContext, "Page adding failed.");

         return false;
      }
   }

   if (!this->size)
   {
      this->firstPageFileOffset = page_offset(page);
      this->firstPageIdx = page->index;

      this->firstPageChunkOffset = _FhgfsChunkPageVec_getChunkPageOffset(this, page->index);
   }

   this->lastPageIdx = page->index;

   this->size++;

   this->lastPageUsedDataSize = usedPageLength;

   return true;
}


/**
 * Get the current vector Size
 */
unsigned FhgfsChunkPageVec_getSize(FhgfsChunkPageVec* this)
{
   return this->size;
}

struct inode* FhgfsChunkPageVec_getInode(FhgfsChunkPageVec* this)
{
      return this->inode;
}


/**
 * Get the offset
 */
loff_t FhgfsChunkPageVec_getFirstPageFileOffset(FhgfsChunkPageVec* this)
{
   return this->firstPageFileOffset;
}


/**
 * Return the next page in the pageListVec.
 * On first call of this function it returns the first page.
 * If there are no more pages left it returns NULL and resets the counters.
 */
FhgfsPage* FhgfsChunkPageVec_iterateGetNextPage(FhgfsChunkPageVec* this)
{
   FhgfsPage* fhgfsPage;

   // printk(KERN_INFO "%s curVecIdx: %zu maxPagesPerVec: %zu size: %u\n",
   //   __func__, this->currentVecIdx, this->maxPagesPerVec, this->size);

   if (this->currentListVecIterIdx == this->size)
      return NULL; // end reached

   if (this->currentVecIdx == this->maxPagesPerVec)
   {   // we reached the last page of this vector, take the next list element
      FhgfsPageListVec* nextListVec;

      if (unlikely(list_is_last(&this->currentIterListVec->list, &this->pageListVecHead) ) )
      {  // last list element reached, reset and return NULL
         Logger* log = App_getLogger(this->app);

         // actually must not happen, as we check above for this->currentVecIdx == this->size
         Logger_logErrFormatted(log, __func__,
            "Bug: Is last list element, but should have more pages: %zu/%u",
            this->currentVecIdx, this->size);

         return NULL;
      }

      nextListVec  = list_next_entry(this->currentIterListVec, list);

      this->currentIterListVec = nextListVec;
      this->currentVecIdx = 0;
   }

   fhgfsPage = FhgfsPageListVec_getFhgfsPage(this->currentIterListVec, this->currentVecIdx);

   this->currentVecIdx++; // increase for the next round
   this->currentListVecIterIdx++;

   return fhgfsPage;
}

/**
 * Reset the iterator, so that one may walk over all pages from the beginning again
 */
void FhgfsChunkPageVec_resetIterator(FhgfsChunkPageVec* this)
{
   this->currentIterListVec = FhgfsChunkPageVec_getFirstPageListVec(this);
   this->currentVecIdx = 0;
   this->currentListVecIterIdx = 0;
}



/**
 * Get the curent overall vector index (as if we would have a single vector and not a list of
 * vectors.
 */
size_t FhgfsChunkPageVec_getCurrentListVecIterIdx(FhgfsChunkPageVec* this)
{
   return this->currentListVecIterIdx;
}


/**
 * Get the overall size of the chunk-vec-list
 */
size_t FhgfsChunkPageVec_getDataSize(FhgfsChunkPageVec* this)
{
   return (this->size - 1) * PAGE_SIZE + this->lastPageUsedDataSize;
}

/**
 * Get the remaining size of the chunk-vec-list
 */
size_t FhgfsChunkPageVec_getRemainingDataSize(FhgfsChunkPageVec* this)
{
   size_t remainingPages = this->size - this->currentListVecIterIdx;

   return (remainingPages - 1) * PAGE_SIZE + this->lastPageUsedDataSize;
}


/**
 * Compute the offset within a single chunk
 */
unsigned _FhgfsChunkPageVec_getChunkPageOffset(FhgfsChunkPageVec* this, pgoff_t index)
{
   unsigned numChunkPages = this->numChunkPages;

      /* note: "& (numChunkPages - 1) only works as "% numChunkPages" replacement,
       *       because chunksize (and therefore also chunkPages) must be a power of two */

      return index & (numChunkPages - 1);
}


#endif // FHGFSCHUNKPAGEVEC_H_
