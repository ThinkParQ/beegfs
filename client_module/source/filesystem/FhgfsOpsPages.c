/*
 * fhgfs page cache methods
 *
 */

#include <app/log/Logger.h>
#include <app/App.h>
#include <app/config/Config.h>
#include <common/toolkit/vector/StrCpyVec.h>
#include <common/toolkit/LockingTk.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/StringTk.h>
#include <common/storage/striping/StripePattern.h>
#include <components/worker/RWPagesWork.h>

#include <filesystem/ProcFs.h>
#include <os/OsTypeConversion.h>
#include "FhgfsOpsDir.h"
#include "FhgfsOpsFile.h"
#include "FhgfsOpsHelper.h"
#include "FhgfsOpsInode.h"
#include "FhgfsOpsPages.h"
#include "FhgfsOpsSuper.h"
#include "FhgfsOps_versions.h"

#include <linux/writeback.h>
#include <linux/mm.h>
#include <linux/mpage.h>
#include <linux/backing-dev.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/mempool.h>

#ifdef CONFIG_COMPAT
#include <asm/compat.h>
#endif

#define INITIAL_FIND_PAGES (16)    // search initially for this number of pages

#define FHGFSOPSPAGES_pageVecListCacheName BEEGFS_MODULE_NAME_STR "-pageListVec"
#define BEEGFS_PAGE_VEC_LIST_POOL_SIZE  8 // number of reserve page list-vecs


struct FhgfsPageData;
typedef struct FhgfsPageData FhgfsPageData;


#if ((INITIAL_FIND_PAGES) > (BEEGFS_MAX_PAGE_LIST_SIZE))
   #error // trigger a compilation error as we would end up with memory corruption in real live
#endif


static struct kmem_cache* FhgfsOpsPages_pageListVecCache = NULL;
static mempool_t*         FhgfsOpsPages_pageListVecPool = NULL;

// forward declarations
struct fhgfsWritePageHelper;
typedef struct fhgfsWritePageHelper fhgfsWrPgHelper;



static int _FhgfsOpsPages_sendPageVec(FhgfsPageData* pageData,
   struct inode* inode, bool isFinal, Fhgfs_RWType rwType);
static FhgfsChunkPageVec* _FhgfsOpsPages_allocNewPageVec(FhgfsPageData* pageData);
static inline FhgfsOpsErr _FhgfsOpsPages_referenceReadFileHandle(FhgfsPageData* writePageData,
   struct file* file);
static inline FhgfsOpsErr _FhgfsOpsPages_referenceWriteFileHandle(FhgfsPageData* writePageData);
static FhgfsOpsErr _FhgfsOpsPages_referenceFileHandle(FhgfsPageData* writePageData,
   unsigned openFlags);

static int _FhgfsOpsPages_writepages(struct address_space* mapping, struct writeback_control* wbc,
   struct page* page);
static int FhgfsOpsPages_writePageCallBack(struct page *page, struct writeback_control *wbc,
   void *data);


static int _FhgfsOpsPages_readpages(struct file* file, struct address_space* mapping,
   struct list_head* pageList, struct page* page);
static int FhgfsOpsPages_readPageCallBack(void *dataPtr, struct page *page);


/**
 * A struct with variables to be exchanged between fhgfs writepages functions
 */
struct FhgfsPageData
{
   struct inode* inode;

   FhgfsChunkPageVec *chunkPageVec;

   bool isReferenced;
   FileHandleType handleType;
   RemotingIOInfo ioInfo;
};


/**
 * Initialize the pageListVecCache and pageListVec mempool
 */
bool FhgfsOpsPages_initPageListVecCache(void)
{
      size_t cacheSize = sizeof(FhgfsPageListVec);

      FhgfsOpsPages_pageListVecCache =
         OsCompat_initKmemCache(FHGFSOPSPAGES_pageVecListCacheName, cacheSize, NULL);

      // create a kmem PageVecList allocation cache
      if (!FhgfsOpsPages_pageListVecCache)
         return false;

      FhgfsOpsPages_pageListVecPool = mempool_create(BEEGFS_PAGE_VEC_LIST_POOL_SIZE,
         mempool_alloc_slab, mempool_free_slab, FhgfsOpsPages_pageListVecCache);

      // create a mempool as last reserve for the PageVecList allocation cache
      if (!FhgfsOpsPages_pageListVecPool)
      {
         kmem_cache_destroy(FhgfsOpsPages_pageListVecCache);
         FhgfsOpsPages_pageListVecCache = NULL;

         return false;
      }

      return true;
}

/**
 * Destroy the pageListVecCache and pageListVec mempool
 */
void FhgfsOpsPages_destroyPageListVecCache(void)
{
   // first destroy the pool, then the cache, as the pool uses cached objects
   if (FhgfsOpsPages_pageListVecPool)
   {
      mempool_destroy(FhgfsOpsPages_pageListVecPool);
      FhgfsOpsPages_pageListVecPool = NULL;
   }

   if (FhgfsOpsPages_pageListVecCache)
   {
      kmem_cache_destroy(FhgfsOpsPages_pageListVecCache);
      FhgfsOpsPages_pageListVecCache = NULL;
   }
}


/**
 * If the meta server has told us for some reason a wrong file-size (i_size) the caller would
 * wrongly discard data beyond i_size. So we are going to correct i_size here.
 *
 * This could be removed once we are sure the meta server *always* has the correct i_size
 * (which might never be the case, due to concurrent writes and truncates).
 *
 * Note: This is the non-inlined version. Only call it from
 *    FhgfsOpsPages_incInodeFileSizeOnPagedRead()
 */
void __FhgfsOpsPages_incInodeFileSizeOnPagedRead(struct inode* inode, loff_t offset, size_t readRes)
{
   App* app = FhgfsOps_getApp(inode->i_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = "Paged-read";
   loff_t i_size;

   FhgfsIsizeHints iSizeHints;

   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   const EntryInfo* entryInfo = FhgfsInode_getEntryInfo(fhgfsInode);

   /* Refresh the inode first, hopefully that is sufficient
    * Note: The inode MUST NOT be flused from this functions, as the caller hold locked pages.
    *       But on flushing the inode, mm/vfs will wait for page unlocks - it would deadlock!
    */
   __FhgfsOps_doRefreshInode(app, inode, NULL, &iSizeHints, true);

   i_size = i_size_read(inode);

   if (unlikely(readRes && (offset + (loff_t)readRes > i_size) ) )
   {  // _refreshInode was not sufficient, force a meta-update
      FhgfsOpsErr refreshRes = FhgfsOpsRemoting_refreshEntry(app, entryInfo);

      if (refreshRes != FhgfsOpsErr_SUCCESS)
      {
         Logger_logErr(log, logContext, "Meta Refresh failed.");
      }

      // again try to refresh the inode, again the inode must not be flushed to avoid deadlocks
      __FhgfsOps_doRefreshInode(app, inode, NULL, &iSizeHints, true);


      /* note on i_lock/i_size: make sure we only increase i_size and do not decrease it
         (e.g. in case we're racing with a concurrent writer at a higher offset) */
      spin_lock(&inode->i_lock); // L O C K

      i_size = i_size_read(inode);

      if (unlikely(offset + (loff_t)readRes > i_size) )
      {  // All attempts to update the remote inode size to the position that we read failed.

         /* i_size_write() sugguests to lock i_mutex, but we might end up here from write_begin()
          * (FhgfsOps_write_begin), which aready has i_mutex locked.  The more common callers
          * readpages() and readpage() do not have i_mutex, though. */

         i_size_write(inode, offset + readRes);

         spin_unlock(&inode->i_lock); // U N L O C K

         /* note: this situation can also be "normal" with a concurrent trunc, so we have to be
            careful regarding user warnings and error return values. */
         Logger_logFormatted(log, Log_DEBUG, logContext, "Failed to increase MDS inode size to "
            "the expected value. (Application might read less data than expected or file was "
            "truncated during read operation. Expected size: %lld isSize: %lld)",
            offset + (loff_t)readRes, i_size);
      }
      else
         spin_unlock(&inode->i_lock); // U N L O C K
   }

   Logger_logFormatted(log, Log_DEBUG, logContext,
      "EntryID: %s Correcting inode size from %lld to %lld",
      entryInfo->entryID, i_size, offset + readRes);
}

/**
 * Just a small wrapper to write a pageVec
 *
 * Note: no-op if writePageData->chunkPageVec is NULL or chunkPageVec has a size of 0
 *
 * Note: Will destroy writePageData->pageVec and create a new pageVec
 *
 * @param isFinalWrite  If true no new pageVec will be allocated
 *
 * @return 0 on success, negative linux error code on error
 */
int _FhgfsOpsPages_sendPageVec(FhgfsPageData* pageData, struct inode* inode, bool isFinal,
   Fhgfs_RWType rwType)
{
   const char* logContext = __func__;
   App* app = FhgfsOps_getApp(inode->i_sb);

   int retVal = 0;
   bool queueSuccess;

   if (!pageData->chunkPageVec)
      return retVal; // nothing to do

   if (FhgfsChunkPageVec_getSize(pageData->chunkPageVec) == 0)
   {  // pageVec is empty
      if (isFinal)
      {
         FhgfsChunkPageVec_destroy(pageData->chunkPageVec);
         pageData->chunkPageVec = NULL;
      }

      return retVal;
   }

   queueSuccess = RWPagesWork_createQueue(app, pageData->chunkPageVec, inode, rwType);
   pageData->chunkPageVec = NULL;

   if (unlikely(!queueSuccess) )
   {
      Logger_logFormattedWithEntryID(inode, Log_ERR, logContext, "Creating the async queue failed");

      retVal = -ENOMEM;
      goto out;
   }

   if (!isFinal)
   {
      FhgfsChunkPageVec* newPageVec;
      // allocate the next chunkPageVec
      newPageVec = _FhgfsOpsPages_allocNewPageVec(pageData);

      if (unlikely(!newPageVec) )
         retVal = -ENOMEM;
   }

out:
   return retVal;
}

/**
 * Just allocate a pageVector
 */
FhgfsChunkPageVec* _FhgfsOpsPages_allocNewPageVec(FhgfsPageData* pageData)
{
   struct inode* inode = pageData->inode;
   App* app = FhgfsOps_getApp(inode->i_sb);
   unsigned chunkPages = RemotingIOInfo_getNumPagesPerChunk(&pageData->ioInfo);
   FhgfsChunkPageVec* newPageVec;

   newPageVec = FhgfsChunkPageVec_create(app, inode, FhgfsOpsPages_pageListVecCache,
      FhgfsOpsPages_pageListVecPool ,chunkPages);

   pageData->chunkPageVec = newPageVec; // assign new pagevec

   return newPageVec;
}

FhgfsOpsErr _FhgfsOpsPages_referenceReadFileHandle(FhgfsPageData* writePageData, struct file* file)
{
   FsFileInfo* fileInfo   = __FhgfsOps_getFileInfo(file);
   unsigned openFlags     = FsFileInfo_getAccessFlags(fileInfo) & OPENFILE_ACCESS_MASK_RW;

   return _FhgfsOpsPages_referenceFileHandle(writePageData, openFlags);
}

FhgfsOpsErr _FhgfsOpsPages_referenceWriteFileHandle(FhgfsPageData* writePageData)
{
   unsigned openFlags = OPENFILE_ACCESS_WRITE;

   return _FhgfsOpsPages_referenceFileHandle(writePageData, openFlags);
}

/**
 * Reference the file if not already referenced and get the handleType
 *
 * @param file may be NULL (from writepages)
 */
FhgfsOpsErr _FhgfsOpsPages_referenceFileHandle(FhgfsPageData* writePageData, unsigned openFlags)
{
   const char* logContext = "OpsPages_writeReferenceFileHandle";
   FhgfsOpsErr referenceRes;

   struct inode* inode = writePageData->inode;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   if (writePageData->isReferenced)
      return FhgfsOpsErr_SUCCESS; // already referenced, just return

   /* we will never truncate the file here, so referenceHandle will not send an event. with
    * no event to send, we don't need to supply a dentry. */
   referenceRes = FhgfsInode_referenceHandle(fhgfsInode, NULL, openFlags, true, NULL,
      &(writePageData->handleType), NULL);

   if (referenceRes != FhgfsOpsErr_SUCCESS)
   {  // failure
      Logger_logFormattedWithEntryID(inode, Log_ERR, logContext,
         "Referencing the file handle failed! Error: %s", FhgfsOpsErr_toErrString(referenceRes) );
   }
   else
   {  // success
      FhgfsInode_getRefIOInfo(fhgfsInode, writePageData->handleType,
         FhgfsInode_handleTypeToOpenFlags(writePageData->handleType), &writePageData->ioInfo);

      writePageData->isReferenced = true;
   }

   return referenceRes;
}


/**
 * Callback for the mm/vfs write_cache_pages function.
 *
 * Collect the given pages into data->pageVec. If data->pageVec cannot take more pages
 * (chunk is full) a write request will be send immediately
 *
 * @return 0 on success, negative linux error code on error
 */
int FhgfsOpsPages_writePageCallBack(struct page *page, struct writeback_control *wbc, void *dataPtr)
{
   const char* logContext = __func__;
   int retVal = 0;

   FhgfsPageData* writePageData = (FhgfsPageData*) dataPtr;

   struct inode* inode = writePageData->inode;

   loff_t fileSize = i_size_read(inode);
   pgoff_t endIndex = fileSize >> PAGE_SHIFT;

   int usedPageLen;

   const bool finalWrite = false; /* When called from this callBack method, finalWrite
                                               * is always false. */

   bool pageVecWasSent = false;

   int referenceRes = _FhgfsOpsPages_referenceWriteFileHandle(writePageData);


   if (referenceRes != FhgfsOpsErr_SUCCESS)
   {
      retVal = FhgfsOpsErr_toSysErr(referenceRes);
      goto outWriteErr;
   }

   // note, only allocate the pageVec after referencing the file!
   if (!writePageData->chunkPageVec)
   {
      FhgfsChunkPageVec* pageVec = _FhgfsOpsPages_allocNewPageVec(writePageData);
      if (unlikely(!pageVec) )
      {
         printk_fhgfs_debug(KERN_INFO, "%s:%d ENOMEM\n", __func__, __LINE__);
         retVal = -ENOMEM;
         goto outAgain;
      }
   }

   if (page->index < endIndex)
      /* in this case, the page is within the limits of the file */
      usedPageLen = PAGE_SIZE;
   else
   {  // the page does not entirely fit into the file size limit
      IGNORE_UNUSED_VARIABLE(logContext);

      usedPageLen = fileSize & ~PAGE_MASK;

      if (page->index > endIndex || !usedPageLen)
      {  // Page is outside the file size limit, probably truncate in progess, ignore this page
         int writeRes;

         #ifdef BEEGFS_DEBUG
         {
            Logger_logFormattedWithEntryID(inode, Log_NOTICE, logContext,
               "Page outside file size limit. file-size: %llu page-offset: %llu, usedPageLen: %d "
               "pg-Idx: %lu endIdx: %lu",
               fileSize, page_offset(page), usedPageLen, page->index, endIndex);
         }
         #endif

         writeRes = _FhgfsOpsPages_sendPageVec(writePageData, inode, finalWrite,
            BEEGFS_RWTYPE_WRITE);
         if (unlikely(writeRes) )
         {

            retVal = writeRes;
            goto outWriteErr;
         }

         if (PageError(page) )
            ClearPageError(page);

         // set- and end page-writeback to remove the page from dirty-page-tree
         set_page_writeback(page);
         end_page_writeback(page);

         // invalidate the page (for reads) as there is a truncate in process
         ClearPageUptodate(page);

         goto outUnlock; // don't re-dirty the page to avoid further write attempts
      }
   }


   while(1) // repeats only once, until pageVecWasSent == true
   {
      int pushSucces;

      pushSucces = FhgfsChunkPageVec_pushPage(writePageData->chunkPageVec, page, usedPageLen);

      if (!pushSucces)
      {  // pageVec probably full, send it and create a new one
         int writeRes;

         if (unlikely(pageVecWasSent) )
         { /* We already send the pageVec once, no need to do it again for an empty vec.
            * Probably of out memory */
            Logger_logFormattedWithEntryID(inode, Log_ERR, logContext,
               "pageVec push failed, page-index: %ld",  writePageData->chunkPageVec->firstPageIdx);

            retVal = -ENOMEM;
            goto outAgain;
         }
         else
         {
            #ifdef BEEGFS_DEBUG
               if (writePageData->chunkPageVec->size == 0)
                  Logger_logFormattedWithEntryID(inode, Log_ERR, logContext,
                     "initial push failed, index: %ld", writePageData->chunkPageVec->firstPageIdx);
            #endif
         }


         writeRes = _FhgfsOpsPages_sendPageVec(writePageData, inode, finalWrite,
            BEEGFS_RWTYPE_WRITE);

         pageVecWasSent = true;

         if (unlikely(writeRes) )
         {
            Logger_logFormattedWithEntryID(inode, Log_ERR, logContext, "ChunkPageVec writing failed.");

            retVal = writeRes;
            goto outWriteErr;
         }

         if (unlikely(!writePageData->chunkPageVec) )
         {
            Logger_logFormattedWithEntryID(inode, Log_ERR, logContext, "Warning, chunkPageVec is NULL.");

            goto outAgain;
         }
         else
         {
            continue; // try again to push the page
         }
      }
      else
      {  // push success
      }

      break; // break on success
   }

   BUG_ON(PageWriteback(page));
   set_page_writeback(page);

   return retVal;


outWriteErr:
   set_bit(AS_EIO, &page->mapping->flags);
   unlock_page(page);
   return retVal;

outAgain: // redirty and unlock the page, it will be handled again
   set_page_dirty(page);

outUnlock:
   unlock_page(page);
   return retVal;
}


/**
 * address_space_operations.writepages method
 *
 * @retVal 0 on success, otherwise negative linux error code
 */
int _FhgfsOpsPages_writepages(struct address_space* mapping, struct writeback_control* wbc,
   struct page* page)
{
   struct inode* inode = mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   int retVal;

   FhgfsPageData pageData =
   {
      .inode         = inode,
      .chunkPageVec  = NULL,
      .isReferenced  = false,
   };

   #ifdef LOG_DEBUG_MESSAGES
   {
      App* app = FhgfsOps_getApp(inode->i_sb);
      struct dentry* dentry = d_find_alias(inode); // calls dget_locked

      FhgfsOpsHelper_logOpDebug(app, dentry, inode, __func__, "(nr_to_write: %ld = %lluKiB)",
         wbc->nr_to_write, (long long) (wbc->nr_to_write << PAGE_SHIFT) / 1024);

      if(dentry)
         dput(dentry);
   }
   #endif // LOG_DEBUG_MESSAGES


   if (!page)
   {  // writepages
      retVal = write_cache_pages(mapping, wbc, FhgfsOpsPages_writePageCallBack, &pageData);
   }
   else
   {  // Called with a single page only, so we are called from ->writepage
      retVal = FhgfsOpsPages_writePageCallBack(page, wbc, &pageData);

      if (unlikely(retVal < 0) )
      {  // some kind of error
         if (unlikely(pageData.chunkPageVec) )
         {
            printk_fhgfs(KERN_ERR,
               "Bug: pageData.chunkPageVec set, but error code returned\n");
            dump_stack();

            // try to clean it up
            FhgfsChunkPageVec_destroy(pageData.chunkPageVec);
            pageData.chunkPageVec = NULL;
         }
      }
   }

   if (pageData.chunkPageVec)
   {
      int writeRes;

      if (unlikely(!pageData.isReferenced) )
      {  // This would be a big bug and should be impossible at all!
         int referenceRes;

         printk_fhgfs(KERN_ERR, "%s: Bug: File is not referenced! ", __func__);
         dump_stack();

         referenceRes = _FhgfsOpsPages_referenceWriteFileHandle(&pageData);
         if (unlikely(referenceRes) != FhgfsOpsErr_SUCCESS)
         {
            retVal = FhgfsOpsErr_toSysErr(referenceRes);
            FhgfsChunkPageVec_iterateAllHandleWritePages(pageData.chunkPageVec, referenceRes);

            FhgfsChunkPageVec_destroy(pageData.chunkPageVec);
            pageData.chunkPageVec = NULL;
            goto outReferenceErr;
         }
      }

      writeRes = _FhgfsOpsPages_sendPageVec(&pageData, inode, true, BEEGFS_RWTYPE_WRITE);

      if (unlikely(writeRes))
         retVal = writeRes;
   }

   if (pageData.isReferenced)
      FhgfsInode_releaseHandle(fhgfsInode, pageData.handleType, NULL);

outReferenceErr:
   #ifdef LOG_DEBUG_MESSAGES
   {
      App* app = FhgfsOps_getApp(inode->i_sb);
      struct dentry* dentry = d_find_alias(inode); // calls dget_locked

      FhgfsOpsHelper_logOpDebug(app, dentry, inode, __func__, "retVal: %d", retVal);

      if(dentry)
         dput(dentry);
   }
   #endif // LOG_DEBUG_MESSAGES

   return retVal;
}

/**
 * @return 0 on success, negative linux error code otherwise
 *
 * NOTE: page is already locked here
 */
int FhgfsOpsPages_writepage(struct page *page, struct writeback_control *wbc)
{
   // Note: this is a writeback method, so we have no file handle here! (only the inode)

   // note: make sure that write-mapping is enabled in fhgfsops_mmap!!!

   #ifdef BEEGFS_DEBUG
      if(!PageUptodate(page) )
         printk_fhgfs_debug(KERN_WARNING, "%s: Bug: page not up-to-date!\n", __func__);
   #endif

   return _FhgfsOpsPages_writepages(page->mapping, wbc, page);
}

int FhgfsOpsPages_writepages(struct address_space* mapping, struct writeback_control* wbc)
{
   #ifdef LOG_DEBUG_MESSAGES
   {
      struct inode* inode = mapping->host;
      App* app = FhgfsOps_getApp(inode->i_sb);
      struct dentry* dentry = d_find_alias(inode); // calls dget_locked

      FhgfsOpsHelper_logOpDebug(app, dentry, inode, __func__, "(nr_to_write: %ld = %lluKiB)",
         wbc->nr_to_write, (long long) (wbc->nr_to_write << PAGE_SHIFT) / 1024);

      if(dentry)
         dput(dentry);
   }
   #endif // LOG_DEBUG_MESSAGES

   return _FhgfsOpsPages_writepages(mapping, wbc, NULL);
}


/**
 * Handle a written page
 *
 * Note: This must be only called once the server acknowledged it received the page
 *       (successful write)
 *
 * @param writeRes positive number means the write succeeded, negative is standard error code
 */
void FhgfsOpsPages_endWritePage(struct page* page, int writeRes, struct inode* inode)
{
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   kunmap(page); // page data access done, so unmap it

   if (unlikely(writeRes <= 0) )
   {  // write error
      if (writeRes == -EAGAIN || writeRes == 0)
         set_page_dirty(page);
      else
      {
         SetPageError(page);
         FhgfsInode_decNumDirtyPages(fhgfsInode);
      }
   }
   else
      FhgfsInode_decNumDirtyPages(fhgfsInode);

   end_page_writeback(page);
   unlock_page(page);
}



/**
 * We don't have data for this page, check if the page exceeds the file size limit
 *
 * Handle a short (sparse) read, which might have 2 two reasons
 * 1) Another client truncated the file, as this client has another inode-size
 *    the kernel read-ahead would try forever to read missing data
 * 2) Sparse files, so one or more chunks have less data than following chunks
 *
 * As we do not know which applies, we always update the inode size first, but calling code
 * shall do this only once per chunk.
 *
 * */
bool FhgfsOpsPages_isShortRead(struct inode* inode, pgoff_t pageIndex,
   bool needInodeRefresh)
{
   bool retVal = false;
   App* app = FhgfsOps_getApp(inode->i_sb);

   off_t  iSize = i_size_read(inode);
   pgoff_t fileEndIndex = iSize >> PAGE_SHIFT;

   FhgfsIsizeHints iSizeHints;

   if (needInodeRefresh && pageIndex > fileEndIndex)
   {  // page is outside the file size limit

      /* don't flush here, this called before FhgfsOpsPages_endReadPage(), so a page lock
       * is being held and if flush would try to lock this page, it deadlocks. */
      __FhgfsOps_doRefreshInode(app, inode, NULL, &iSizeHints, true);
      needInodeRefresh = false;

      iSize = i_size_read(inode);
      fileEndIndex = iSize >> PAGE_SHIFT;
   }

   if (pageIndex < fileEndIndex)
      retVal = true;

   return retVal;
}

/**
 * Note: Needs to be called for all pages added to pageVec in FhgfsOps_readpagesVec() in order to
 *       set the status and to unlock it.
 *
 * @param readRes the number of read bytes or negative linux error code
 */
void FhgfsOpsPages_endReadPage(Logger* log, struct inode* inode, struct FhgfsPage* fhgfsPage,
   int readRes)
{
   struct page* page = fhgfsPage->page;
   loff_t offset = FhgfsPage_getFileOffset(fhgfsPage);

   const char* logContext = __func__;

   /* LogTopic_COMMKIT here, as the message belongs better to CommKitVec, although it is a page
    * relate method */
   Logger_logTopFormatted(log, LogTopic_COMMKIT, Log_SPAM, logContext,
      "Page-index: %lu readRes: %d", page->index, readRes);

   FhgfsOpsPages_incInodeFileSizeOnPagedRead(inode, offset, readRes);


   if (readRes < 0)
      SetPageError(page);
   else
   { /* note: There is no way to mark a page outside the filesize, so even with readRes == 0
      *       we need to SetPageUptodate(page) */

      if (readRes && readRes < PAGE_SIZE)
      {
         // zero the remainder of the page for which we don't have data

         // zero_user_segment() would be optimal, but not available in older kernels
         // zero_user_segment(page, zeroOffset, BEEGFS_PAGE_SIZE);
         // BUT we can use our kmapped Fhgfs_Page directly for zeroing

         memset(fhgfsPage->data + readRes, 0, PAGE_SIZE - readRes);
      }

      flush_dcache_page(page);
      SetPageUptodate(page);
   }

   FhgfsPage_unmapUnlockReleasePage(page);
}


/**
 * Callback for the mm/vfs read_cache_pages function.
 *
 * Collect the given pages into data->pageVec. If data->pageVec cannot take more pages
 * (chunk is full) a read request will be send immediately.
 *
 * @return 0 on success, negative linux error code on error
 */
int FhgfsOpsPages_readPageCallBack(void *dataPtr, struct page *page)
{
   const char* logContext = __func__;
   int retVal = 0;

   FhgfsPageData* pageData = (FhgfsPageData*) dataPtr;

   struct inode* inode = pageData->inode;

   bool pageVecWasSent = false;

   const bool finalRead = false; // is not the final read from this function

   // note, only allocate the pageVec after referencing the file!
   if (!pageData->chunkPageVec)
   {
      FhgfsChunkPageVec* pageVec = _FhgfsOpsPages_allocNewPageVec(pageData);
      if (unlikely(!pageVec) )
      {
         printk_fhgfs_debug(KERN_INFO, "%s:%d ENOMEM\n", __func__, __LINE__);
         retVal = -ENOMEM;
         goto outErr;
      }
   }

   while(1) // repeats only once, until pageVecWasSent == true
   {
      bool pushSucces;

      pushSucces = FhgfsChunkPageVec_pushPage(pageData->chunkPageVec, page, PAGE_SIZE);

      if (!pushSucces)
      {  // pageVec probably full, send it and create a new one
         int readRes;

         if (unlikely(pageVecWasSent) )
         { /* We already send the pageVec once, no need to do it again for an empty vec.
            * Probably of out memory */
            Logger_logFormattedWithEntryID(inode, Log_ERR, logContext,
               "pageVec push failed, page-index: %ld",  pageData->chunkPageVec->firstPageIdx);

            retVal = -ENOMEM;
            goto outErr;
         }
         else
         {
            #ifdef BEEGFS_DEBUG
               if (pageData->chunkPageVec->size == 0)
                  Logger_logFormattedWithEntryID(inode, Log_ERR, logContext,
                     "initial push failed, index: %ld", pageData->chunkPageVec->firstPageIdx);
            #endif
         }


         readRes = _FhgfsOpsPages_sendPageVec(pageData, inode, finalRead, BEEGFS_RWTYPE_READ);

         pageVecWasSent = true;

         if (unlikely(readRes) )
         {
            Logger_logFormattedWithEntryID(inode, Log_ERR, logContext,
               "ChunkPageVec writing failed.");

            retVal = readRes;
            goto outErr;
         }

         if (unlikely(!pageData->chunkPageVec) )
         {
            Logger_logFormattedWithEntryID(inode, Log_ERR, logContext,
               "Warning, chunkPageVec is NULL.");

            retVal = -ENOMEM;
            goto outErr;
         }
         else
         {
            continue; // try again to push the page
         }
      }
      else
      {  // push success
      }

      break; // break on success
   }

   get_page(page); // reference page to avoid it is re-used while we read it
   return retVal;


outErr:
   return retVal;
}

/**
 * Read a page synchronously
 *
 * Note: Reads a locked page and returns it locked again (page will be unlocked in between)
 *
 */
int FhgfsOpsPages_readpageSync(struct file* file, struct page* page)
{
   int retVal;

   ClearPageUptodate(page);

   retVal = FhgfsOpsPages_readpage(file, page); // note: async read will unlock the page
   if (retVal)
   {
      lock_page(page); // re-lock it
      return retVal; // some kind of error
   }

   lock_page(page); // re-lock it

   if (!PageUptodate(page) )
   {
      /* The uptodate flag is not set, which means reading the page failed with an io-error.
       * Note: FhgfsOpsPages_readpage *must* SetPageUptoDate even if the page does not exist on
       *       the server (i.e. file size too small), as other mm/vfs code requires that.
       *       So if it is not set there was some kind of IO error. */
      retVal = -EIO;
   }

   return retVal;
}

/*
 * Read a single page
 * Note: Called with the page locked and we need to unlock it when done.
 *
 * Note: Caller holds a reference to this page
 * Note: Reading the page is done asynchronously from another thread
 *
 * @return 0 on success, negative linux error code otherwise
 */
int FhgfsOpsPages_readpage(struct file* file, struct page* page)
{
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   struct inode* inode = file_inode(file);
   int writeBackRes;

   int retVal;

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, __func__,
         "page-index: %lu", page->index);
   IGNORE_UNUSED_VARIABLE(app);

   writeBackRes = FhgfsOpsPages_writeBackPage(inode, page);
   if (writeBackRes)
   {
      retVal = writeBackRes;
      goto outErr;
   }

   retVal = _FhgfsOpsPages_readpages(file, file->f_mapping, NULL, page);

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, __func__, "page-index: %lu retVal: %d",
      page->index, retVal);

   return retVal;

outErr:
   unlock_page(page);

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, __func__, "page-index: %lu retVal: %d",
      page->index, retVal);

   return retVal;
}


/**
 * Read a list of pages or a single page
 *
 * @param pageList/page  either of both must be NULL
 * @param retVal 0 on success, otherwise negative linux error code
 */
int _FhgfsOpsPages_readpages(struct file* file, struct address_space* mapping,
   struct list_head* pageList, struct page* page)
{
   struct inode* inode = mapping->host;
   int referenceRes;

   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   int retVal = 0;

   FhgfsPageData pageData =
   {
      .inode         = inode,
      .chunkPageVec  = NULL,
      .isReferenced  = false,
   };


   referenceRes = _FhgfsOpsPages_referenceReadFileHandle(&pageData, file);
   if (unlikely(referenceRes) != FhgfsOpsErr_SUCCESS)
   {
      retVal = FhgfsOpsErr_toSysErr(referenceRes);

      return retVal;
   }


   ihold(inode); // make sure the inode is not released

   if (pageList)
   {  // classical readpages, we get a list pages, which are not in the page cache yet
      retVal = read_cache_pages(mapping, pageList, FhgfsOpsPages_readPageCallBack, &pageData);
   }
   else
   { /* Called with a single page only, that does not need to be added to the cache,
      * we are called from ->readpage */

      retVal = FhgfsOpsPages_readPageCallBack(&pageData, page);

      if (unlikely(retVal < 0) )
      {  // some kind of error
         if (unlikely(pageData.chunkPageVec) )
         {
            printk_fhgfs(KERN_ERR,
               "Bug: pageData.chunkPageVec set, but error code returned\n");
            dump_stack();

            // try to clean it up
            FhgfsChunkPageVec_destroy(pageData.chunkPageVec);
            pageData.chunkPageVec = NULL;
         }

         // unlock the page as it cannot be handled.
         unlock_page(page);
      }
   }

   if (pageData.chunkPageVec)
   {
      int readRes;

      if (unlikely(!pageData.isReferenced) )
      {  // This would be a big bug and should be impossible at all!
         printk_fhgfs(KERN_ERR, "%s: Bug: File is not referenced! ", __func__);
         dump_stack();

      }

      readRes = _FhgfsOpsPages_sendPageVec(&pageData, inode, true, BEEGFS_RWTYPE_READ);

      if (unlikely(readRes))
         retVal = readRes;
   }

   if (likely(pageData.isReferenced) )
      FhgfsInode_releaseHandle(fhgfsInode, pageData.handleType, NULL);

   iput(inode);

   return retVal;
}

/**
 * address_space_operations.readpages method
 */
int FhgfsOpsPages_readpages(struct file* file, struct address_space* mapping,
   struct list_head* pageList, unsigned numPages)
{
   struct dentry* dentry = file_dentry(file);
   struct inode* inode = mapping->host;
   App* app = FhgfsOps_getApp(dentry->d_sb);

   const char* logContext = __func__;

   FhgfsOpsHelper_logOpDebug(app, dentry, inode, logContext, "(num_pages: %u)", numPages);
   IGNORE_UNUSED_VARIABLE(logContext);
   IGNORE_UNUSED_VARIABLE(app);
   IGNORE_UNUSED_VARIABLE(inode);

   return _FhgfsOpsPages_readpages(file, mapping, pageList, NULL);
}


/*
 * Write back all requests on one page - we do this before reading it.
 *
 * Note: page is locked and must stay locked
 */
int FhgfsOpsPages_writeBackPage(struct inode *inode, struct page *page)
{
   struct writeback_control wbc;
   int ret;

   loff_t range_start = page_offset(page);
   loff_t range_end = range_start + (loff_t)(PAGE_SIZE - 1);


   memset(&wbc, 0, sizeof(wbc) );
   wbc.sync_mode = WB_SYNC_ALL;
   wbc.nr_to_write = 0;

   wbc.range_start = range_start;
   wbc.range_end = range_end;

   IGNORE_UNUSED_VARIABLE(range_start);
   IGNORE_UNUSED_VARIABLE(range_end);

   for (;;) {
      wait_on_page_writeback(page);
      if (clear_page_dirty_for_io(page))
      {
         ret = FhgfsOpsPages_writepage(page, &wbc); // note: unlocks the page
         lock_page(page); // re-lock the page
         if (ret < 0)
            goto out_error;
         continue;
      }
      ret = 0;
      if (!PagePrivate(page))
         break;
   }

out_error:
   return ret;
}


