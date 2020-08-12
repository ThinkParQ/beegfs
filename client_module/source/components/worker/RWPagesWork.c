#include <app/log/Logger.h>
#include <app/App.h>
#include <filesystem/FhgfsOpsPages.h>

#include "RWPagesWork.h"

#define RWPagesWorkQueue_SUB_NAME BEEGFS_MODULE_NAME_STR "-rwPgWQ" // read-pages-work-queue

static struct workqueue_struct* rwPagesWorkQueue = NULL;


static void RWPagesWork_processQueue(RWPagesWork* this);
static bool RWPagesWork_queue(RWPagesWork *this);

static FhgfsOpsErr _RWPagesWork_initReferenceFile(struct inode* inode, Fhgfs_RWType rwType,
   FileHandleType* outHandleType, RemotingIOInfo* outIOInfo);

bool RWPagesWork_initworkQueue(void)
{
   rwPagesWorkQueue = create_workqueue(RWPagesWorkQueue_SUB_NAME);

   return !!rwPagesWorkQueue;
}

void RWPagesWork_destroyWorkQueue(void)
{
   if (rwPagesWorkQueue)
   {
      flush_workqueue(rwPagesWorkQueue);
      destroy_workqueue(rwPagesWorkQueue);
   }
}

void RWPagesWork_flushWorkQueue(void)
{
   if (rwPagesWorkQueue)
      flush_workqueue(rwPagesWorkQueue);
}


bool RWPagesWork_queue(RWPagesWork *this)
{
   return queue_work(rwPagesWorkQueue, &this->kernelWork);
}

bool RWPagesWork_init(RWPagesWork* this, App* app, struct inode* inode,
   FhgfsChunkPageVec *pageVec, Fhgfs_RWType rwType)
{
   FhgfsOpsErr referenceRes;

   this->app           = app;
   this->inode         = inode;
   this->pageVec       = pageVec;
   this->rwType        = rwType;

   referenceRes = _RWPagesWork_initReferenceFile(inode, rwType, &this->handleType, &this->ioInfo);

   if (unlikely(referenceRes != FhgfsOpsErr_SUCCESS) )
      return false;

   INIT_WORK(&this->kernelWork, RWPagesWork_process);

   return true;
}

/**
 * Init helper function to reference a file.
 *
 * Note: The file is already supposed to be referenced by the FhgfsOpsPages_readpages or
 *       FhgfsOpsPages_writepages, so file referencing is not supposed to fail
 */
FhgfsOpsErr _RWPagesWork_initReferenceFile(struct inode* inode, Fhgfs_RWType rwType,
   FileHandleType* outHandleType, RemotingIOInfo* outIOInfo)
{
   FhgfsOpsErr referenceRes;

   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   int openFlags = (rwType == BEEGFS_RWTYPE_WRITE) ? OPENFILE_ACCESS_WRITE : OPENFILE_ACCESS_READ;

   referenceRes = FhgfsInode_referenceHandle(fhgfsInode, NULL, openFlags, true, NULL,
      outHandleType, NULL);

   if (unlikely(referenceRes != FhgfsOpsErr_SUCCESS) )
   {  // failure
      printk_fhgfs(KERN_INFO, "Bug: file not referenced");
      dump_stack();
   }
   else
   {  // success

      //get the right openFlags (might have changed to OPENFILE_ACCESS_READWRITE)
      openFlags = FhgfsInode_handleTypeToOpenFlags(*outHandleType);

      FhgfsInode_getRefIOInfo(fhgfsInode, *outHandleType, openFlags, outIOInfo);
   }

   return referenceRes;
}


/**
 * Process the work queue
 */
void RWPagesWork_process(struct work_struct* work)
{
   RWPagesWork* thisCast = (RWPagesWork*)work;

   RWPagesWork_processQueue(thisCast);
}

/**
 * Needed for old INIT_WORK() with 3 parameters (before 2.6.20)
 */
void RWPagesWork_oldProcess(void* data)
{
   struct work_struct* work = (struct work_struct*) data;

   return RWPagesWork_process(work);
}

/**
 * Build worker queues
 */
bool RWPagesWork_createQueue(App* app, FhgfsChunkPageVec* pageVec, struct inode* inode,
    Fhgfs_RWType rwType)
{
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   bool retVal = true;

   RWPagesWork* work;

   work = RWPagesWork_construct(app, inode, pageVec, rwType);
   if (likely(work) )
   {
      bool queueRes;

      queueRes = RWPagesWork_queue(work);
      if (!queueRes)
      {
         Logger_logErr(log, logContext, "RWPagesWork_construct failed.");

         if (rwType == BEEGFS_RWTYPE_READ)
            FhgfsChunkPageVec_iterateAllHandleReadErr(pageVec);
         else
            FhgfsChunkPageVec_iterateAllHandleWritePages(pageVec, -EIO);

         RWPagesWork_destruct(work);
      }
   }

   if (unlikely(!work))
   {  // Creating the work-queue failed

      Logger_logErr(log, logContext, "Failed to create work queue.");

      retVal = false;
   }

   return retVal;

}

/**
 * Process a request from the queue
 */
void RWPagesWork_processQueue(RWPagesWork* this)
{
   App* app = this->app;
   Logger* log = App_getLogger(app);

   ssize_t rwRes;

   rwRes = FhgfsOpsRemoting_rwChunkPageVec(this->pageVec, &this->ioInfo,  this->rwType);

   if (unlikely(rwRes < 0) )
      LOG_DEBUG_FORMATTED(log, 1, __func__, "error: %s", FhgfsOpsErr_toErrString(-rwRes) );
   else
   {
      LOG_DEBUG_FORMATTED(log, 5, __func__, "rwRes: %zu", rwRes );
      IGNORE_UNUSED_VARIABLE(log);
   }

   RWPagesWork_destruct(this);
}


