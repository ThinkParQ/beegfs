#ifndef FHGFSOPSHELPER_H_
#define FHGFSOPSHELPER_H_

#include <app/App.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include <components/InternodeSyncer.h>
#include <filesystem/FsDirInfo.h>
#include <filesystem/FsFileInfo.h>
#include <net/filesystem/FhgfsOpsRemoting.h>


#ifdef LOG_DEBUG_MESSAGES
   /**
    * Print debug messages. Used to trace functions which are frequently called and therefore
    * has to be exlicitly enabled at compilation time.
    * Has to be a macro due to usage of __VA_ARGS__.
    */
   #define FhgfsOpsHelper_logOpDebug(app, dentry, inode, logContext, msgStr, ...) \
      FhgfsOpsHelper_logOpMsg(Log_SPAM, app, dentry, inode, logContext, msgStr, ##__VA_ARGS__)
#else
   // no debug build, so those debug messages disabled at all
   #define FhgfsOpsHelper_logOpDebug(app, dentry, inode, logContext, msgStr, ...)
#endif // LOG_DEBUG_MESSAGES



extern void FhgfsOpsHelper_logOpMsg(int level, App* app, struct dentry* dentry, struct inode* inode,
   const char *logContext, const char *msgStr, ...);

extern int FhgfsOpsHelper_refreshDirInfoIncremental(App* app, const EntryInfo* entryInfo,
   FsDirInfo* dirInfo, bool forceUpdate);

extern FhgfsOpsErr FhgfsOpsHelper_flushCache(App* app, FhgfsInode* fhgfsInode,
   bool discardCacheOnError);
extern FhgfsOpsErr FhgfsOpsHelper_flushCacheNoWait(App* app, FhgfsInode* fhgfsInode,
   bool discardCacheOnError);
extern ssize_t FhgfsOpsHelper_writeCached(struct iov_iter *iter, size_t size,
   loff_t offset, FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo);
extern ssize_t FhgfsOpsHelper_readCached(struct iov_iter *iter, size_t size,
   loff_t offset, FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo);

extern FhgfsOpsErr __FhgfsOpsHelper_flushCacheUnlocked(App* app, FhgfsInode* fhgfsInode,
   bool discardCacheOnError);
extern ssize_t __FhgfsOpsHelper_writeCacheFlushed(struct iov_iter *iter, size_t size,
   loff_t offset, FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo);
extern ssize_t __FhgfsOpsHelper_readCacheFlushed(struct iov_iter *iter, size_t size,
   loff_t offset, FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo);
extern void __FhgfsOpsHelper_discardCache(App* app, FhgfsInode* fhgfsInode);

extern FhgfsOpsErr FhgfsOpsHelper_getAppendLock(FhgfsInode* inode, RemotingIOInfo* ioInfo);
extern FhgfsOpsErr FhgfsOpsHelper_releaseAppendLock(FhgfsInode* inode, RemotingIOInfo* ioInfo);

ssize_t FhgfsOpsHelper_appendfileVecOffset(FhgfsInode* fhgfsInode, struct iov_iter *iter,
      size_t count, RemotingIOInfo* ioInfo, loff_t offsetFromEnd, loff_t* outNewOffset);

extern FhgfsOpsErr FhgfsOpsHelper_readOrClearUser(App* app, struct iov_iter *iter, size_t size,
   loff_t offset, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo);

extern void FhgfsOpsHelper_getRelativeLinkStr(const char* pathFrom, const char* pathTo,
   char** pathRelativeTo);
extern int FhgfsOpsHelper_symlink(App* app, const EntryInfo* parentInfo, const char* to,
   struct CreateInfo *createInfo, EntryInfo* outEntryInfo);


extern ssize_t FhgfsOpsHelper_readStateless(App* app, const EntryInfo* entryInfo,
   struct iov_iter *iter, size_t size, loff_t offset);
extern ssize_t FhgfsOpsHelper_writeStateless(App* app, const EntryInfo* entryInfo,
   struct iov_iter *iter, size_t size, loff_t offset, unsigned uid, unsigned gid);


// inliners
static inline void FhgfsOpsHelper_logOp(int level, App* app, struct dentry* dentry,
   struct inode* inode, const char *logContext);
static inline FhgfsOpsErr FhgfsOpsHelper_closefileWithAsyncRetry(const EntryInfo* entryInfo,
   RemotingIOInfo* ioInfo, struct FileEvent* event);
static inline FhgfsOpsErr FhgfsOpsHelper_unlockEntryWithAsyncRetry(const EntryInfo* entryInfo,
   RWLock* eiRLock, RemotingIOInfo* ioInfo, int64_t clientFD);
static inline FhgfsOpsErr FhgfsOpsHelper_unlockRangeWithAsyncRetry(const EntryInfo* entryInfo,
   RWLock* eiRLock, RemotingIOInfo* ioInfo, int ownerPID);

/**
 * Reads a symlink.
 *
 * @return number of read bytes or negative linux error code
 */
static inline int FhgfsOpsHelper_readlink_kernel(App* app, const EntryInfo* entryInfo,
      char *buf, int size)
{
   struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_KVEC(buf, size, READ);
   return FhgfsOpsHelper_readStateless(app, entryInfo, iter, size, 0);
}


/**
 * Wrapper function for FhgfsOpsHelper_logOpMsg(), just skips msgStr and sets it to NULL
 */
void FhgfsOpsHelper_logOp(int level, App* app, struct dentry* dentry, struct inode* inode,
   const char *logContext)
{
   Logger* log = App_getLogger(app);

   // check the log level here, as this function is inlined
   if (likely(level > Logger_getLogLevel(log) ) )
      return;

   FhgfsOpsHelper_logOpMsg(level, app, dentry, inode, logContext, NULL);
}

/**
 * Call close remoting and add close operation to the corresponding async retry queue if a
 * communucation error occurred.
 *
 * @param entryInfo will be copied
 * @param ioInfo will be copied
 * @return remoting result (independent of whether the operation was added for later retry)
 */
FhgfsOpsErr FhgfsOpsHelper_closefileWithAsyncRetry(const EntryInfo* entryInfo,
   RemotingIOInfo* ioInfo, struct FileEvent* event)
{
   FhgfsOpsErr closeRes;

   closeRes = FhgfsOpsRemoting_closefile(entryInfo, ioInfo, event);

   if( (closeRes == FhgfsOpsErr_COMMUNICATION) || (closeRes == FhgfsOpsErr_INTERRUPTED) )
   { // comm error => add for later retry
      InternodeSyncer* syncer = App_getInternodeSyncer(ioInfo->app);
      InternodeSyncer_delayedCloseAdd(syncer, entryInfo, ioInfo, event);
   }
   else if (event)
      FileEvent_uninit(event);

   return closeRes;
}

FhgfsOpsErr FhgfsOpsHelper_unlockEntryWithAsyncRetry(const EntryInfo* entryInfo, RWLock* eiRLock,
   RemotingIOInfo* ioInfo, int64_t clientFD)
{
   FhgfsOpsErr unlockRes = FhgfsOpsRemoting_flockEntryEx(entryInfo, eiRLock, ioInfo->app,
      ioInfo->fileHandleID, clientFD, 0, ENTRYLOCKTYPE_CANCEL, true);

   if( (unlockRes == FhgfsOpsErr_COMMUNICATION) || (unlockRes == FhgfsOpsErr_INTERRUPTED) )
   { // comm error => add for later retry

      InternodeSyncer* syncer = App_getInternodeSyncer(ioInfo->app);
      InternodeSyncer_delayedEntryUnlockAdd(syncer, entryInfo, ioInfo, clientFD);
   }

   return unlockRes;
}

FhgfsOpsErr FhgfsOpsHelper_unlockRangeWithAsyncRetry(const EntryInfo* entryInfo, RWLock* eiRLock,
   RemotingIOInfo* ioInfo, int ownerPID)
{
   FhgfsOpsErr unlockRes = FhgfsOpsRemoting_flockRangeEx(entryInfo, eiRLock, ioInfo->app,
      ioInfo->fileHandleID, ownerPID, ENTRYLOCKTYPE_CANCEL, 0, ~0ULL, true);

   if( (unlockRes == FhgfsOpsErr_COMMUNICATION) || (unlockRes == FhgfsOpsErr_INTERRUPTED) )
   { // comm error => add for later retry
      InternodeSyncer* syncer = App_getInternodeSyncer(ioInfo->app);
      InternodeSyncer_delayedRangeUnlockAdd(syncer, entryInfo, ioInfo, ownerPID);
   }

   return unlockRes;
}


#endif /*FHGFSOPSHELPER_H_*/
