#include <app/log/Logger.h>
#include <app/App.h>
#include <common/threading/AtomicInt.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/vector/StrCpyVec.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>
#include <filesystem/FhgfsInode.h>
#include <filesystem/FhgfsOpsHelper.h>
#include <filesystem/FsDirInfo.h>
#include <toolkit/InodeRefStore.h>
#include <toolkit/NoAllocBufferStore.h>
#include <os/iov_iter.h>
#include "FhgfsOpsDir.h"
#include "FhgfsOpsHelper.h"


/**
 * Log file system operations and optionally additional messages.
 * Used to trace function calls and to print the path the function is to operate on.
 *
 * @param level         The log level
 * @param dentry        Common vfs directory entry
 * @param logContext    Usually the name of the calling function
 * @param msgStr        Optional message string, may be NULL
 * @param ...           Optional arguments according to format given in msgStr
 */
void FhgfsOpsHelper_logOpMsg(int level, App* app, struct dentry* dentry, struct inode* inode,
   const char *logContext, const char *msgStr, ...)
{
   Logger* log = App_getLogger(app);
   NoAllocBufferStore* bufStore = App_getPathBufStore(app);

   char* pathStoreBuf = NULL; // NoAllocBufferStore_addBuf() can detect a wrong NULL...
   char* path = NULL;
   const char* entryID = NULL;

   const char* noPath = "n/a (no dentry)";
   const char* noEntryID = "n/a (no inode)";


   if(level > Logger_getLogLevel(log) )
      return;


   if(dentry)
   {
      path = __FhgfsOps_pathResolveToStoreBuf(bufStore, dentry, &pathStoreBuf);
      if(IS_ERR(path) )
         path = NULL;
   }

   if(inode)
   { // get entryInfo lock for entryID

      FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
      const EntryInfo* entryInfo;

      FhgfsInode_entryInfoReadLock(fhgfsInode); // L O C K entryInfo

      entryInfo = FhgfsInode_getEntryInfo(fhgfsInode);

      entryID = EntryInfo_getEntryID(entryInfo);
   }

   if(msgStr)
   { // generate new msg string for given msg formatting
      va_list ap;
      char* newMsg;

      newMsg = kmalloc(LOGGER_LOGBUF_SIZE, GFP_NOFS);
      if(newMsg)
      {
         int prefixLen = snprintf(newMsg, LOGGER_LOGBUF_SIZE, "called. Path: %s; EntryID: %s; %s",
               path ? path : noPath,
               entryID ? entryID : noEntryID,
               msgStr);

         va_start(ap, msgStr);
         vsnprintf(newMsg + prefixLen, LOGGER_LOGBUF_SIZE - prefixLen, msgStr, ap);
         va_end(ap);

         Logger_logFormatted(log, level, logContext, "%s", newMsg);

         kfree(newMsg);
      }
      else
      { // alloc failed, still try to log the operation at least
         Logger_logFormatted(log, level, logContext, "called. Path: %s; EntryID: %s; (msg n/a)",
            path ? path : noPath,
            entryID ? entryID : noEntryID);
      }

   }
   else
      Logger_logFormatted(log, level, logContext, "called. Path: %s; EntryID: %s",
         path ? path : noPath,
         entryID ? entryID : noEntryID);


   if(inode)
   { // release entryInfo lock
      FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

      FhgfsInode_entryInfoReadUnlock(fhgfsInode); // U N L O C K entryInfo
   }


   if(pathStoreBuf)
      NoAllocBufferStore_addBuf(bufStore, pathStoreBuf);
}

/**
 * Refreshes the dirInfo by retrieving a new version from the nodes (but only if required or
 * forced).
 *
 * Note: This method guarantees either valid local names range for currentServerOffset or empty
 * dirContents list on completion.
 *
 * @param dirInfo holds the relevant serverOffset for the refresh
 * @param forceUpdate use this to force an update (e.g. when the user seeked)
 * @return negative linux error code on error, 0 otherwise
 */
int FhgfsOpsHelper_refreshDirInfoIncremental(App* app, const EntryInfo* entryInfo,
   FsDirInfo* dirInfo, bool forceUpdate)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOpsHelper (refresh dir info incremental)";

   const unsigned maxNames = 100; // max number of retrieved names

   int retVal = 0;
   FhgfsOpsErr listRes = FhgfsOpsErr_SUCCESS;
   StrCpyVec* dirContents = FsDirInfo_getDirContents(dirInfo);
   size_t dirContentsLen = StrCpyVec_length(dirContents);
   size_t currentContentsPos = FsDirInfo_getCurrentContentsPos(dirInfo); // pos inside contents vec

   LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext, "called. EntryID: %s, Owner: %hu",
      entryInfo->entryID, entryInfo->owner.node.value);
   IGNORE_UNUSED_VARIABLE(log);
   IGNORE_UNUSED_VARIABLE(logContext);


   if(forceUpdate)
   { // user seeked backwards (or something else makes an update necessary)
      LOG_DEBUG(log, Log_SPAM, logContext, "forced update of dir contents");

      listRes = FhgfsOpsRemoting_listdirFromOffset(entryInfo, dirInfo, maxNames);
   }
   else
   if(FsDirInfo_getEndOfDir(dirInfo) && (currentContentsPos >= dirContentsLen) )
   { // we reached the end of the dir contents => do nothing
      LOG_DEBUG(log, Log_SPAM, logContext, "reached end of dir contents by offset");

      StrCpyVec_clear(FsDirInfo_getDirContents(dirInfo) );
   }
   else
   if(currentContentsPos < dirContentsLen)
   { // inside the local contents region => do nothing
      LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext,
         "offset inside local contents region: %lld <= %lld",
         (long long)currentContentsPos, (long long)dirContentsLen);
   }
   else
   { // initial retrieval or offset outside current local contents region
      LOG_DEBUG(log, Log_SPAM, logContext, "retrieving by offset");

      listRes = FhgfsOpsRemoting_listdirFromOffset(entryInfo, dirInfo, maxNames);
   }


   if(unlikely(listRes != FhgfsOpsErr_SUCCESS) )
   { // error
      StrCpyVec_clear(FsDirInfo_getDirContents(dirInfo) );
      FsDirInfo_setCurrentContentsPos(dirInfo, 0);

      Logger_logFormatted(log, Log_DEBUG, logContext, "result: %s",
         FhgfsOpsErr_toErrString(listRes) );

      retVal = FhgfsOpsErr_toSysErr(listRes);
   }

   return retVal;
}


/**
 * Flush cache buffer and return it to the store.
 *
 * @param discardCacheOnError true to discard a write cache if the remote write
 * was not successful; false to just release it to the store in this case
 */
FhgfsOpsErr FhgfsOpsHelper_flushCache(App* app, FhgfsInode* fhgfsInode,
   bool discardCacheOnError)
{
   FhgfsOpsErr retVal;

   FhgfsInode_fileCacheExclusiveLock(fhgfsInode); // L O C K

   retVal = __FhgfsOpsHelper_flushCacheUnlocked(app, fhgfsInode, discardCacheOnError);

   FhgfsInode_fileCacheExclusiveUnlock(fhgfsInode); // U N L O C K

   return retVal;
}

/**
 * Flush cache buffer and return it to the store, but return immediately if the cache lock cannot
 * be acquired immediately.
 *
 * @param discardCacheOnError true to discard a write cache if the remote write
 * was not successful; false to just release it to the store in this case
 * @return FhgfsOpsErr_INUSE if cache lock is contended and nothing has been flushed.
 */
FhgfsOpsErr FhgfsOpsHelper_flushCacheNoWait(App* app, FhgfsInode* fhgfsInode,
   bool discardCacheOnError)
{
   int gotLock; // 1 if we got the lock, 0 otherwise
   FhgfsOpsErr retVal;

   gotLock = FhgfsInode_fileCacheExclusiveTryLock(fhgfsInode); // (T R Y) L O C K
   if(gotLock != 1)
      return FhgfsOpsErr_INUSE;

   retVal = __FhgfsOpsHelper_flushCacheUnlocked(app, fhgfsInode, discardCacheOnError);

   FhgfsInode_fileCacheExclusiveUnlock(fhgfsInode); // U N L O C K

   return retVal;
}

/**
 * Flush cache buffer and return it to the store.
 *
 * Note: Unlocked, so caller must hold inode cache lock.
 *
 * @param discardCacheOnError true to discard a write cache if the remote write
 * was not successful; false to just release it to the store in this case
 */
FhgfsOpsErr __FhgfsOpsHelper_flushCacheUnlocked(App* app, FhgfsInode* fhgfsInode,
   bool discardCacheOnError)
{
   /* note: we don't take a handle to an open file here, because we wouldn't know whether the
      given handle is a read-only handle that just needs to flush the write cache to establish a new
      read cache. that's why we use the _writeStatlessInode() method here. */

   CacheBuffer* cacheBuffer = Fhgfsinode_getFileCacheBuffer(fhgfsInode);
   enum FileBufferType cacheType = cacheBuffer->bufType;
   ssize_t writeRes;

   if(cacheType == FileBufferType_NONE)
      return FhgfsOpsErr_SUCCESS;

   if(cacheType == FileBufferType_READ)
   { // file has a read cache => just discard the cached data
      __FhgfsOpsHelper_discardCache(app, fhgfsInode);
      return FhgfsOpsErr_SUCCESS;
   }

   // file has a write cache => send the cached data to the storage nodes

   writeRes = FhgfsOpsHelper_writeStatelessInode(fhgfsInode, cacheBuffer->buf,
      cacheBuffer->bufUsageLen, cacheBuffer->fileOffset);
   if(unlikely(writeRes < (ssize_t)cacheBuffer->bufUsageLen) )
   { // write did not succeed
      if(discardCacheOnError)
         __FhgfsOpsHelper_discardCache(app, fhgfsInode);

      if(writeRes > 0)
         return FhgfsOpsErr_NOSPACE;

      return -writeRes;
   }

   // write succeeded

   __FhgfsOpsHelper_discardCache(app, fhgfsInode);

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Buffered writing.
 * Tries to add written data to an existing cache or flushes the cache (if any) and delegates to
 * writeCacheFlushed().
 *
 * Note: This method also updates FsFileInfo cache hits counter.
 *
 * @param offset offset in file, -1 for append
 * @return number of bytes written or negative fhgfs error code
 */
ssize_t FhgfsOpsHelper_writeCached(const char __user *buf, size_t size,
   loff_t offset, FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo)
{
   App* app = ioInfo->app;
   NoAllocBufferStore* cacheStore = App_getCacheBufStore(app);

   ssize_t retVal;
   bool isAppendWrite = (offset == -1); // true if this is an append write
   CacheBuffer* cacheBuffer;
   enum FileBufferType cacheType;
   bool isAppendCacheBuf; // true if the current cache buffer is an append write buffer
   size_t bufLenLeft; // remaining buffer length
   loff_t currentCacheEndOffset;
   int userBufCopyRes;

   if (app->cfg->tuneCoherentBuffers)
   {
      i_mmap_lock_read(fhgfsInode->vfs_inode.i_mapping);

      if (beegfs_hasMappings(&fhgfsInode->vfs_inode))
      {
         FsFileInfo_decCacheHits(fileInfo);
         i_mmap_unlock_read(fhgfsInode->vfs_inode.i_mapping);

         return FhgfsOpsHelper_writefileEx(fhgfsInode, buf, size, offset, ioInfo);
      }

      i_mmap_unlock_read(fhgfsInode->vfs_inode.i_mapping);
   }

   FhgfsInode_fileCacheExclusiveLock(fhgfsInode); // L O C K

   cacheBuffer = Fhgfsinode_getFileCacheBuffer(fhgfsInode);
   cacheType = cacheBuffer->bufType;

   if(cacheType == FileBufferType_NONE)
   { // file doesn't have any cache buffer currently

      // update cache hits by guessing whether this would have been a cache hit if we had a buffer

      if(isAppendWrite)
         FsFileInfo_incCacheHits(fileInfo); // could have been a cache hit
      else
      {
         loff_t virtualCacheStart = FsFileInfo_getLastWriteOffset(fileInfo);
         loff_t virtualCacheEnd = virtualCacheStart + NoAllocBufferStore_getBufSize(cacheStore) - 1;

         if( (offset >= virtualCacheStart) && (offset <= virtualCacheEnd) )
            FsFileInfo_incCacheHits(fileInfo); // could have been a cache hit
         else
            FsFileInfo_decCacheHits(fileInfo); // would probably not have been a cache hit
      }

      retVal = __FhgfsOpsHelper_writeCacheFlushed(buf, size, offset, fhgfsInode, fileInfo, ioInfo);
      goto unlock_and_exit;
   }

   if(cacheType == FileBufferType_READ)
   { // file has a read cache => just discard the cached data
      FsFileInfo_decCacheHits(fileInfo);

      __FhgfsOpsHelper_discardCache(app, fhgfsInode);

      retVal = __FhgfsOpsHelper_writeCacheFlushed(buf, size, offset, fhgfsInode, fileInfo, ioInfo);
      goto unlock_and_exit;
   }

   // file has a write cache => can we extend it or do we have to flush it?

   isAppendCacheBuf = cacheBuffer->fileOffset == -1; // -1 offset means append buf
   currentCacheEndOffset = cacheBuffer->fileOffset + cacheBuffer->bufUsageLen;
   bufLenLeft = cacheBuffer->bufUsageMaxLen - cacheBuffer->bufUsageLen;

   if( (isAppendWrite != isAppendCacheBuf) ||
       (!isAppendWrite && (offset != currentCacheEndOffset) ) ||
       (size > bufLenLeft) )
   { // offset doesn't fit or not enough cache room left => flush cache
      FhgfsOpsErr flushRes;

      FsFileInfo_decCacheHits(fileInfo);

      flushRes = __FhgfsOpsHelper_flushCacheUnlocked(app, fhgfsInode, false);
      if(unlikely(flushRes != FhgfsOpsErr_SUCCESS) )
      { // flush failed
         retVal = -flushRes;
         goto unlock_and_exit;
      }

      retVal = __FhgfsOpsHelper_writeCacheFlushed(buf, size, offset, fhgfsInode, fileInfo, ioInfo);
      goto unlock_and_exit;
   }

   // offset fits and we have enough room left for the write => copy data to cache buf (and update)

   FsFileInfo_incCacheHits(fileInfo);

   userBufCopyRes = __copy_from_user(&(cacheBuffer->buf)[cacheBuffer->bufUsageLen], buf, size);
   if(unlikely(userBufCopyRes) )
   { // copy failed
      Logger* log = App_getLogger(app);
      Logger_log(log, Log_DEBUG, __func__, "Buffer copy from userspace failed (invalid buffer)");

      retVal = -FhgfsOpsErr_ADDRESSFAULT;
      goto unlock_and_exit;
   }

   cacheBuffer->bufUsageLen += size;

   bufLenLeft = cacheBuffer->bufUsageMaxLen - cacheBuffer->bufUsageLen; // re-calc remaining bufLen

   // write has been completely cached => check whether the cache is full now

   if(!bufLenLeft)
   { // cache buf used up => flush it
      FhgfsOpsErr flushRes = __FhgfsOpsHelper_flushCacheUnlocked(app, fhgfsInode, false);

      if(unlikely(flushRes != FhgfsOpsErr_SUCCESS) )
      { // flush failed
         retVal = -flushRes;
         goto unlock_and_exit;
      }
   }

   // write was successful if we got here

   retVal = size;


unlock_and_exit:
   FhgfsInode_fileCacheExclusiveUnlock(fhgfsInode); // U N L O C K

   return retVal;
}

/**
 * Buffered reading.
 * Tries to read data from an existing cache or flushes the cache and delegates to
 * readCacheFlushed().
 *
 * @return number of bytes read or negative fhgfs error code
 */
ssize_t FhgfsOpsHelper_readCached(char __user *buf, size_t size, loff_t offset,
   FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo)
{
   App* app = ioInfo->app;
   NoAllocBufferStore* cacheStore = App_getCacheBufStore(app);

   ssize_t retVal;
   CacheBuffer* cacheBuffer;
   enum FileBufferType cacheType;
   loff_t readEndOffset = offset + size - 1;
   loff_t cacheEndOffset;
   ssize_t remoteReadSize;
   loff_t cacheCopyOffset;
   size_t cacheCopySize;
   int userBufCopyRes;

   if (app->cfg->tuneCoherentBuffers)
   {
      i_mmap_lock_read(fhgfsInode->vfs_inode.i_mapping);

      if (beegfs_hasMappings(&fhgfsInode->vfs_inode))
      {
         FsFileInfo_decCacheHits(fileInfo);
         i_mmap_unlock_read(fhgfsInode->vfs_inode.i_mapping);

         return FhgfsOpsRemoting_readfile(buf, size, offset, ioInfo, fhgfsInode);
      }

      i_mmap_unlock_read(fhgfsInode->vfs_inode.i_mapping);
   }


   // fast path for parallel readers (other path takes exclusive lock)
   if(FhgfsInode_getIsFileOpenByMultipleReaders(fhgfsInode) ||
      !FsFileInfo_getAllowCaching(fileInfo) )
   { // no caching needed
      ssize_t readRes;

      FhgfsInode_fileCacheSharedLock(fhgfsInode); // L O C K (shared)

      cacheBuffer = Fhgfsinode_getFileCacheBuffer(fhgfsInode);
      cacheType = cacheBuffer->bufType;

      if(cacheType != FileBufferType_NONE)
      { // file already has buffer attached, so we need exclusive lock
         FhgfsInode_fileCacheSharedUnlock(fhgfsInode); // U N L O C K (shared)
         goto exclusive_lock_path;
      }

      readRes = FhgfsOpsRemoting_readfile(buf, size, offset, ioInfo, fhgfsInode);

      FhgfsInode_fileCacheSharedUnlock(fhgfsInode); // U N L O C K (shared)

      return readRes;
   }


exclusive_lock_path:

   FhgfsInode_fileCacheExclusiveLock(fhgfsInode); // L O C K (exclusive)

   cacheBuffer = Fhgfsinode_getFileCacheBuffer(fhgfsInode);
   cacheType = cacheBuffer->bufType;

   if(cacheType == FileBufferType_NONE)
   { // file has no buffer attached

      // update cache hits by guessing whether this would have been a cache hit if we had a buffer
      loff_t virtualCacheStart = FsFileInfo_getLastReadOffset(fileInfo);
      loff_t virtualCacheEnd = virtualCacheStart + NoAllocBufferStore_getBufSize(cacheStore) - 1;

      if( (virtualCacheEnd >= offset) && (virtualCacheStart <= readEndOffset) )
         FsFileInfo_incCacheHits(fileInfo); // range overlap => could have been a cache hit
      else
         FsFileInfo_decCacheHits(fileInfo); // would probably not have been a cache hit

      retVal = __FhgfsOpsHelper_readCacheFlushed(buf, size, offset, fhgfsInode, fileInfo, ioInfo);
      goto unlock_and_exit;
   }

   if(cacheType == FileBufferType_WRITE)
   { // file has a write cache => flush it
      FhgfsOpsErr flushRes;

      FsFileInfo_decCacheHits(fileInfo);

      flushRes = __FhgfsOpsHelper_flushCacheUnlocked(app, fhgfsInode, false);

      if(unlikely(flushRes != FhgfsOpsErr_SUCCESS) )
      { // flush failed
         retVal = -flushRes;
         goto unlock_and_exit;
      }

      retVal = __FhgfsOpsHelper_readCacheFlushed(buf, size, offset, fhgfsInode, fileInfo, ioInfo);
      goto unlock_and_exit;
   }

   // file has a read cache => does it overlap with the read range?

   cacheEndOffset = cacheBuffer->fileOffset + cacheBuffer->bufUsageLen - 1;

   if( (cacheEndOffset < offset) ||
       (cacheBuffer->fileOffset > readEndOffset) )
   { // cache range and read range do not overlap => flush
      FhgfsOpsErr flushRes;

      FsFileInfo_decCacheHits(fileInfo);

      flushRes = __FhgfsOpsHelper_flushCacheUnlocked(app, fhgfsInode, false);

      if(unlikely(flushRes != FhgfsOpsErr_SUCCESS) )
      { // flush failed
         retVal = -flushRes;
         goto unlock_and_exit;
      }

      retVal = __FhgfsOpsHelper_readCacheFlushed(buf, size, offset, fhgfsInode, fileInfo, ioInfo);
      goto unlock_and_exit;
   }

   // cache range and read range are overlapping. three possible cases to be handled:
   // read range before/inside/behind the cache range

   FsFileInfo_incCacheHits(fileInfo);

   remoteReadSize = 0;

   if(offset < cacheBuffer->fileOffset)
   { // read begins before cache => remote-read and copy the rest
      ssize_t readRes;

      remoteReadSize = cacheBuffer->fileOffset - offset;

      readRes = FhgfsOpsRemoting_readfile(buf, remoteReadSize, offset, ioInfo, fhgfsInode);
      if(readRes < remoteReadSize)
      { // error or end-of-file => invalidate cache
         __FhgfsOpsHelper_discardCache(app, fhgfsInode);

         retVal = readRes;
         goto unlock_and_exit;
      }
   }

   // remote read (if any) succeeded => copy part from cache
   cacheCopyOffset = (offset <= cacheBuffer->fileOffset) ? 0 : (offset - cacheBuffer->fileOffset);
   cacheCopySize = MIN(cacheBuffer->fileOffset + cacheBuffer->bufUsageLen, offset + size) -
      (cacheBuffer->fileOffset + cacheCopyOffset);

   userBufCopyRes = __copy_to_user(
      &buf[remoteReadSize], &(cacheBuffer->buf)[cacheCopyOffset], cacheCopySize);
   if(unlikely(userBufCopyRes) )
   { // copy failed
      Logger* log = App_getLogger(app);
      Logger_log(log, Log_DEBUG, __func__, "Buffer copy to userspace failed (invalid buffer)");

      retVal = -FhgfsOpsErr_ADDRESSFAULT;
      goto unlock_and_exit;
   }


   // has everything been read or is there a remainder behind the cacheBuf?

   if(readEndOffset > cacheEndOffset)
   { // there is a remainder behind the cacheBuf => discard cache (to allow new caching)
      loff_t remainderBufOffset = remoteReadSize + cacheCopySize;
      loff_t remainderFileOffset = cacheEndOffset + 1;
      size_t remainderSize = readEndOffset - cacheEndOffset;
      ssize_t remainderRes;

      __FhgfsOpsHelper_discardCache(app, fhgfsInode);

      remainderRes = __FhgfsOpsHelper_readCacheFlushed(&buf[remainderBufOffset],
         remainderSize, remainderFileOffset, fhgfsInode, fileInfo, ioInfo);
      if(unlikely(remainderRes < 0) )
      { // reading failed
         retVal = remainderRes;
         goto unlock_and_exit;
      }

      retVal = remoteReadSize + cacheCopySize + remainderRes;
      goto unlock_and_exit;
   }

   // read was successful if we got here

   retVal = size;


unlock_and_exit:
   FhgfsInode_fileCacheExclusiveUnlock(fhgfsInode); // U N L O C K (exclusive)

   return retVal;
}



/**
 * Try to cache the write data or delegate to the normal remoting writefile().
 *
 * Note: Use this only when definitely no file cache entry exists.
 * Note: Unlocked, so caller must hold inode cache lock.
 *
 * @param offset offset in file, -1 for append
 * @return number of bytes written or negative fhgfs error code
 */
ssize_t __FhgfsOpsHelper_writeCacheFlushed(const char __user *buf,
   size_t size, loff_t offset, FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo)
{
   App* app = ioInfo->app;
   NoAllocBufferStore* cacheStore = App_getCacheBufStore(app);
   InodeRefStore* refStore = App_getInodeRefStore(app);

   bool isAppendWrite = (offset == -1); // true if this is an append write
   StripePattern* pattern = ioInfo->pattern;
   size_t maxCacheLen;
   CacheBuffer* cacheBuffer;
   int userBufCopyRes;

   // caching disabled?
   if(!FsFileInfo_getAllowCaching(fileInfo) ||
      (!isAppendWrite && (FsFileInfo_getCacheHits(fileInfo) <= 0) ) )
      return FhgfsOpsHelper_writefileEx(fhgfsInode, buf, size, offset, ioInfo);


   // check whether the write size is larger than cacheBuf or would span multiple chunks

   if(isAppendWrite)
      maxCacheLen = NoAllocBufferStore_getBufSize(cacheStore);
   else
   { // normal write (not append)
      size_t currentChunkSize = StripePattern_getChunkEnd(pattern, offset) - offset + 1;

      maxCacheLen = MIN(currentChunkSize, NoAllocBufferStore_getBufSize(cacheStore) );
   }

   if(size >= maxCacheLen) // (Note: '=' because we don't want a completely filled up buffer)
   { // scenario not allowed => write through
      return FhgfsOpsHelper_writefileEx(fhgfsInode, buf, size, offset, ioInfo);
   }


   // we got something to cache here (data fits completely into a cacheBuf)

   // create new cache (if buffer available)

   cacheBuffer = Fhgfsinode_getFileCacheBuffer(fhgfsInode);

   #ifdef BEEGFS_DEBUG
      BEEGFS_BUG_ON(cacheBuffer->buf, "Looks like we're about to leak a cache buffer");
   #endif // BEEGFS_DEBUG

   cacheBuffer->buf = NoAllocBufferStore_instantBuf(cacheStore);
   if(!cacheBuffer->buf)
   { // no cache buffer left in the store => write through
      return FhgfsOpsHelper_writefileEx(fhgfsInode, buf, size, offset, ioInfo);
   }

   cacheBuffer->bufType = FileBufferType_WRITE; // (needed here for _discardCache() below)

   // init cache entry fields and copy data to cacheBuf
   userBufCopyRes = __copy_from_user(cacheBuffer->buf, buf, size);
   if(unlikely(userBufCopyRes) )
   { // copy failed
      Logger* log = App_getLogger(app);
      Logger_log(log, Log_DEBUG, __func__, "Buffer copy from userspace failed (invalid buffer)");

      __FhgfsOpsHelper_discardCache(app, fhgfsInode);
      return -FhgfsOpsErr_ADDRESSFAULT;
   }

   cacheBuffer->bufUsageLen = size;
   cacheBuffer->bufUsageMaxLen = maxCacheLen;
   cacheBuffer->fileOffset = offset; // (note: can be -1 in case of append)

   // add to inode ref store for async flush
   InodeRefStore_addAndReferenceInode(refStore, BEEGFS_VFSINODE(fhgfsInode) );

   return size;
}


/**
 * Try to read-ahead data or just do a plain normal remoting readfile().
 *
 * Note: Use this only when definitely no file cache entry exists.
 * Note: Unlocked, so caller must hold inode cache lock.
 *
 * @return number of bytes read or negative fhgfs error code
 */
ssize_t __FhgfsOpsHelper_readCacheFlushed(char __user *buf, size_t size, loff_t offset,
   FhgfsInode* fhgfsInode, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo)
{
   App* app = ioInfo->app;
   NoAllocBufferStore* cacheStore = App_getCacheBufStore(app);
   InodeRefStore* refStore = App_getInodeRefStore(app);

   StripePattern* pattern = ioInfo->pattern;
   size_t currentChunkSize;
   size_t maxCacheLen;
   CacheBuffer* cacheBuffer;
   ssize_t readRes;
   int userBufCopyRes;


   // caching disabled?
   if(!FsFileInfo_getAllowCaching(fileInfo) || (FsFileInfo_getCacheHits(fileInfo) <= 0) ||
      FhgfsInode_getIsFileOpenByMultipleReaders(fhgfsInode) )
      return FhgfsOpsRemoting_readfile(buf, size, offset, ioInfo, fhgfsInode);

   // check whether the read size is larger than a single cacheBuf or would span multiple chunks
   currentChunkSize = StripePattern_getChunkEnd(pattern, offset) - offset + 1;
   maxCacheLen = MIN(currentChunkSize, NoAllocBufferStore_getBufSize(cacheStore) );
   if(size >= maxCacheLen) // (Note: '=' because we don't want a completely used up buffer)
   { // scenario not allowed => read direct
      return FhgfsOpsRemoting_readfile(buf, size, offset, ioInfo, fhgfsInode);
   }

   // looks like we got something to cache here (read size is smaller than a single cacheBuf)

   // create new cache (if buffer available)

   cacheBuffer = Fhgfsinode_getFileCacheBuffer(fhgfsInode);

   #ifdef BEEGFS_DEBUG
      BEEGFS_BUG_ON(cacheBuffer->buf, "Looks like we're about to leak a cache buffer");
   #endif // BEEGFS_DEBUG

   cacheBuffer->buf = NoAllocBufferStore_instantBuf(cacheStore);
   if(!cacheBuffer->buf)
   { // no cache buffer left in the store => read direct
      return FhgfsOpsRemoting_readfile(buf, size, offset, ioInfo, fhgfsInode);
   }

   cacheBuffer->bufType = FileBufferType_READ; // (needed here for _discardCache() below)

   // we got a buffer => read as much as possible into the cache buffer

   if(!offset && (size < FSFILEINFO_CACHE_SLOWSTART_READLEN) )
      maxCacheLen = FSFILEINFO_CACHE_SLOWSTART_READLEN; /* reduce read-ahead at file start for small
         reads. good if e.g. a process is only looking at file starts */


   readRes = FhgfsOpsRemoting_readfile(cacheBuffer->buf, maxCacheLen, offset, ioInfo, fhgfsInode);
   if(readRes <= 0)
   { // error or immediate end of file
      __FhgfsOpsHelper_discardCache(app, fhgfsInode);
      return readRes;
   }

   // init cache entry fields and copy data from cache buffer to user buffer
   userBufCopyRes = __copy_to_user(buf, cacheBuffer->buf, MIN(size, (size_t)readRes) );
   if(unlikely(userBufCopyRes) )
   { // copy failed
      Logger* log = App_getLogger(app);
      Logger_log(log, Log_DEBUG, __func__, "Buffer copy to userspace failed (invalid buffer)");

      __FhgfsOpsHelper_discardCache(app, fhgfsInode);
      return -FhgfsOpsErr_ADDRESSFAULT;
   }

   cacheBuffer->bufUsageLen = readRes;
   cacheBuffer->bufUsageMaxLen = maxCacheLen;
   cacheBuffer->fileOffset = offset;

   // add to inode ref store for async flush
   InodeRefStore_addAndReferenceInode(refStore, BEEGFS_VFSINODE(fhgfsInode) );

   return MIN(size, (size_t)readRes);
}

/**
 * Discard the current cache buffer and return it to the store.
 *
 * Note: Unlocked, so caller must hold the inode cache lock.
 */
void __FhgfsOpsHelper_discardCache(App* app, FhgfsInode* fhgfsInode)
{
   NoAllocBufferStore* cacheStore = App_getCacheBufStore(app);
   InodeRefStore* refStore = App_getInodeRefStore(app);

   CacheBuffer* cacheBuffer = Fhgfsinode_getFileCacheBuffer(fhgfsInode);

   #ifdef BEEGFS_DEBUG
   if( (cacheBuffer->buf == NULL) || (cacheBuffer->bufType == FileBufferType_NONE) )
   {
      BEEGFS_BUG_ON(1, "Attempting to discard an invalid cache buffer");
      return;
   }
   #endif // BEEGFS_DEBUG

   NoAllocBufferStore_addBuf(cacheStore, cacheBuffer->buf);

   cacheBuffer->buf = NULL; // (NULL'ing required for debug sanity checks)
   cacheBuffer->bufType = FileBufferType_NONE;

   // remove inode from async flush store
   InodeRefStore_removeAndReleaseInode(refStore, BEEGFS_VFSINODE(fhgfsInode) );
}

/**
 * Wrapper for FhgfsOpsRemoting_writefile, but this automatically calls _appendfile() if -1 offset
 * is given.
 *
 * @param offset offset in file, -1 for append
 */
ssize_t FhgfsOpsHelper_writefileEx(FhgfsInode* fhgfsInode, const char __user *buf, size_t size,
   loff_t offset, RemotingIOInfo* ioInfo)
{
   if(offset == -1)
      return FhgfsOpsHelper_appendfile(fhgfsInode, buf, size, ioInfo, &offset);
   else
      return FhgfsOpsRemoting_writefile(buf, size, offset, ioInfo);
}

FhgfsOpsErr FhgfsOpsHelper_getAppendLock(FhgfsInode* inode, RemotingIOInfo* ioInfo)
{
   FhgfsOpsErr lockRes;

   FhgfsInode_entryInfoReadLock(inode); // LOCK EntryInfo

   lockRes = FhgfsOpsRemoting_flockAppendEx(&inode->entryInfo, &inode->entryInfoLock, ioInfo->app,
      ioInfo->fileHandleID, 0, current->pid, ENTRYLOCKTYPE_EXCLUSIVE, true);

   FhgfsInode_entryInfoReadUnlock(inode); // UNLOCK EntryInfo

   if(unlikely(lockRes != FhgfsOpsErr_SUCCESS) )
   {
      LOG_DEBUG_FORMATTED(App_getLogger(ioInfo->app), Log_DEBUG, __func__, "Append lock error: %s",
         FhgfsOpsErr_toErrString(lockRes) );
      SAFE_ASSIGN(ioInfo->needsAppendLockCleanup, true);
   }

   return lockRes;
}

FhgfsOpsErr FhgfsOpsHelper_releaseAppendLock(FhgfsInode* inode, RemotingIOInfo* ioInfo)
{
   FhgfsOpsErr unlockRes;

   FhgfsInode_entryInfoReadLock(inode); // LOCK EntryInfo

   unlockRes = FhgfsOpsRemoting_flockAppendEx(&inode->entryInfo, &inode->entryInfoLock, ioInfo->app,
      ioInfo->fileHandleID, 0, current->pid, ENTRYLOCKTYPE_UNLOCK, true);

   FhgfsInode_entryInfoReadUnlock(inode); // UNLOCK EntryInfo

   if(unlikely(unlockRes != FhgfsOpsErr_SUCCESS) )
      SAFE_ASSIGN(ioInfo->needsAppendLockCleanup, true);

   return unlockRes;
}

/**
 * Append data to a file, protected by MDS locking.
 *
 * Note: This method does not try to flush local file buffers after acquiring the MDS lock, because
 * this method might be called during a file buffer flush (so callers must ensure that there are
 * no conflicing local file buffers).
 *
 * @param size buffer length to be appended
 * @param outNewOffset new file offset after append completes (only valid if no error returned)
 * @return number of bytes written or negative fhgfs error code
 */
ssize_t FhgfsOpsHelper_appendfile(FhgfsInode* fhgfsInode, const char __user *buf, size_t size,
   RemotingIOInfo* ioInfo, loff_t* outNewOffset)
{
   struct iovec iov = {
      .iov_len = size,
      .iov_base = (void*) buf
   };

   return FhgfsOpsHelper_appendfileVecOffset(fhgfsInode, &iov, 1, ioInfo, 0, outNewOffset);
}

ssize_t FhgfsOpsHelper_appendfileVecOffset(FhgfsInode* fhgfsInode, const struct iovec* data,
   size_t dataCount, RemotingIOInfo* ioInfo, loff_t offsetFromEnd, loff_t* outNewOffset)
{
   App* app = ioInfo->app;

   ssize_t writeRes = 0;
   FhgfsOpsErr lockRes;
   FhgfsOpsErr statRes;
   fhgfs_stat fhgfsStat;

   // get MDS append lock...
   lockRes = FhgfsOpsHelper_getAppendLock(fhgfsInode, ioInfo);
   if(unlikely(lockRes != FhgfsOpsErr_SUCCESS) )
      return -lockRes;

   // get current file size from servers...

   FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo

   statRes = FhgfsOpsRemoting_statDirect(app, FhgfsInode_getEntryInfo(fhgfsInode), &fhgfsStat);

   FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo

   if(unlikely(statRes != FhgfsOpsErr_SUCCESS) )
   { // remote stat error
      writeRes = -statRes;
      goto unlock_and_exit;
   }

   // the actual remote write...

   fhgfsStat.size += offsetFromEnd;

   while(dataCount > 0)
   {
      writeRes = FhgfsOpsRemoting_writefile(data->iov_base, data->iov_len, fhgfsStat.size, ioInfo);
      if(writeRes < 0)
         goto unlock_and_exit_some_written;

      fhgfsStat.size += writeRes;
      dataCount--;
      data++;
   }

unlock_and_exit_some_written:
   *outNewOffset = fhgfsStat.size;

unlock_and_exit:
   FhgfsOpsHelper_releaseAppendLock(fhgfsInode, ioInfo);
   return writeRes;
}


/**
 * Reads chunk by chunk from the servers and zero-fills the buffer if a server returns EOF.
 * So the caller must make sure that the file actually has at least the requested size.
 *
 * Note: Intended for sparse file reading.
 * Note: There is also a similar version for kernel buffers.
 */
FhgfsOpsErr FhgfsOpsHelper_readOrClearUser(App* app, char __user *buf, size_t size,
   loff_t offset, FsFileInfo* fileInfo, RemotingIOInfo* ioInfo)
{
   size_t toBeRead = size;
   loff_t currentOffset = offset;
   StripePattern* pattern = ioInfo->pattern;

   while(toBeRead)
   {
      size_t currentBufOffset = size - toBeRead;
      size_t currentChunkSize = StripePattern_getChunkEnd(
         pattern, currentOffset) - currentOffset + 1;
      size_t currentReadSize = MIN(currentChunkSize, toBeRead);
      ssize_t currentReadRes;

      currentReadRes = FhgfsOpsRemoting_readfile(&buf[currentBufOffset], currentReadSize,
         currentOffset, ioInfo, NULL);

      if(unlikely(currentReadRes < 0) )
         return -currentReadRes;

      if( (size_t)currentReadRes < currentReadSize)
      { // zero-fill the remainder
         long clearVal;

         clearVal = clear_user(&buf[currentBufOffset + currentReadRes],
            currentReadSize - currentReadRes);
         if (clearVal != 0)
            return FhgfsOpsErr_ADDRESSFAULT;
      }

      currentOffset += currentReadSize;
      toBeRead -= currentReadSize;
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Takes two paths that are relative to the mount point and creates a new path that is relative from
 * pathFromStr to pathToStr.
 *
 * @param outPathRelativeToStr will be kalloc'ed and needs to be kfree'd by the caller
 */
void FhgfsOpsHelper_getRelativeLinkStr(const char* pathFromStr, const char* pathToStr,
   char** outPathRelativeToStr)
{
   // the idea here is to prepend a "../" to pathTo for every parent dir of pathFrom

   int i;
   int pathFromLen; // (must be signed, because it might be negative for certain paths)
   Path pathFrom;
   Path pathTo; // will be modified to become the relative result path
   StrCpyList* pathFromElems;
   StrCpyList* pathToElems;

   Path_initFromString(&pathFrom, pathFromStr);
   Path_initFromString(&pathTo, pathToStr);

   pathFromElems = Path_getPathElems(&pathFrom);
   pathToElems = Path_getPathElems(&pathTo);

   pathFromLen = StrCpyList_length(pathFromElems);

   // insert a ".." for every parent dir in pathFrom

   // (note: -1 to excluse the final path element)
   for(i = 0; i < (pathFromLen-1); i++)
   {
      StrCpyList_addHead(pathToElems, "..");
   }

   Path_setAbsolute(&pathTo, false);

   *outPathRelativeToStr = Path_getPathAsStrCopy(&pathTo);

   Path_uninit(&pathFrom);
   Path_uninit(&pathTo);
}

/**
 * Note: creates the symlink as a normal file and writes the target to it
 *
 * @param mode access mode (permission flags)
 * @return 0 on success, negative linux error code otherwise
 */
int FhgfsOpsHelper_symlink(App* app, const EntryInfo* parentInfo, const char* to,
   struct CreateInfo* createInfo, EntryInfo* outEntryInfo)
{
   int retVal = 0;

   size_t toStrLen = strlen(to);
   FhgfsOpsErr mkRes;
   AtomicInt maxUsedTargetIndex;
   RemotingIOInfo ioInfo;
   FhgfsOpsErr openRes;
   size_t numTargets;
   ssize_t writeRes;
   FhgfsOpsErr closeRes;
   BitStore firstWriteDone;
   PathInfo pathInfo;
   const struct FileEvent* event;

   AtomicInt_init(&maxUsedTargetIndex, -1);

   // create the file

   // stash createInfo->fileEvent. we want to send it only during close (when the symlink is fully
   // created) - mkfile would generate its own event if createInfo->fileEvent was set.
   event = createInfo->fileEvent;
   createInfo->fileEvent = NULL;
   mkRes = FhgfsOpsRemoting_mkfile(app, parentInfo, createInfo, outEntryInfo);
   createInfo->fileEvent = event;
   if(mkRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(mkRes);
      goto err_exit;
   }

   // open the file

   memset(&pathInfo, 0, sizeof(pathInfo) );

   RemotingIOInfo_initOpen(app, OPENFILE_ACCESS_WRITE, &maxUsedTargetIndex, &pathInfo, &ioInfo);

   openRes = FhgfsOpsRemoting_openfile(outEntryInfo, &ioInfo, NULL, NULL);
   if(openRes != FhgfsOpsErr_SUCCESS)
   { // error
      FhgfsOpsRemoting_unlinkfile(app, parentInfo, createInfo->entryName, NULL);

      retVal = FhgfsOpsErr_toSysErr(openRes);
      goto err_cleanup_open;
   }

   numTargets = UInt16Vec_length(ioInfo.pattern->getStripeTargetIDs(ioInfo.pattern));
   BitStore_initWithSizeAndReset(&firstWriteDone, numTargets);
   ioInfo.firstWriteDone = &firstWriteDone;
   ioInfo.userID = createInfo->userID;
   ioInfo.groupID = createInfo->groupID;

   // write link-destination to the file

   writeRes = FhgfsOpsRemoting_writefile(to, toStrLen, 0, &ioInfo);
   if(writeRes < (ssize_t)toStrLen)
   { // error
      FhgfsOpsHelper_closefileWithAsyncRetry(outEntryInfo, &ioInfo, NULL);
      FhgfsOpsRemoting_unlinkfile(app, parentInfo, createInfo->entryName, NULL);

      retVal = (writeRes < 0) ? FhgfsOpsErr_toSysErr(-writeRes) : FhgfsOpsErr_INTERNAL;
      goto err_cleanup_open;
   }


   // close the file

   // callee frees fileEvent and wants a non-const pointer to signify this. callers of _symlink
   // must be aware of this and not free the fileEvent themselves
   closeRes = FhgfsOpsHelper_closefileWithAsyncRetry(outEntryInfo, &ioInfo,
         (struct FileEvent*) createInfo->fileEvent);

   if(closeRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(closeRes);
      // createInfo->fileEvent has been taken care of, don't free it again
      createInfo->fileEvent = NULL;
      goto err_cleanup_open;
   }


   // clean-up

   BitStore_uninit(&firstWriteDone);
   RemotingIOInfo_freeVals(&ioInfo);

   return retVal;

err_cleanup_open:
   if (ioInfo.firstWriteDone)
      BitStore_uninit(ioInfo.firstWriteDone);

   RemotingIOInfo_freeVals(&ioInfo);
   EntryInfo_uninit(outEntryInfo);

   if (createInfo->fileEvent)
      FileEvent_uninit((struct FileEvent*) createInfo->fileEvent);

err_exit:
   return retVal;
}

/**
 * Reads a symlink.
 *
 * @return number of read bytes or negative linux error code
 */
int FhgfsOpsHelper_readlink(App* app, const EntryInfo* entryInfo, char __user* buf, int size)
{
   return FhgfsOpsHelper_readStateless(app, entryInfo, buf, size, 0);
}

/**
 * Opens a file (the file must exist), reads data from it and closes it.
 *
 * Note: This is really slow currently, because of the open/close overhead.
 *
 * @return number of read bytes or negative linux error code
 */
ssize_t FhgfsOpsHelper_readStateless(App* app, const EntryInfo* entryInfo,
   char __user *buf, size_t size, loff_t offset)
{
   int retVal = -EREMOTEIO;

   AtomicInt maxUsedTargetIndex;
   RemotingIOInfo ioInfo;
   FhgfsOpsErr openRes;
   ssize_t readRes;
   size_t numTargets;
   FhgfsOpsErr closeRes;
   BitStore firstWriteDone;
   PathInfo pathInfo;

   AtomicInt_init(&maxUsedTargetIndex, -1);

   // open file

   memset(&pathInfo, 0, sizeof(pathInfo) );

   RemotingIOInfo_initOpen(app, OPENFILE_ACCESS_READ, &maxUsedTargetIndex, &pathInfo, &ioInfo);

   openRes = FhgfsOpsRemoting_openfile(entryInfo, &ioInfo, NULL, NULL);

   if(openRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(openRes);
      goto clean_up_open;
   }

   numTargets = UInt16Vec_length(ioInfo.pattern->getStripeTargetIDs(ioInfo.pattern));
   BitStore_initWithSizeAndReset(&firstWriteDone, numTargets);
   ioInfo.firstWriteDone = &firstWriteDone;

   // read file

   readRes = FhgfsOpsRemoting_readfile(buf, size, offset, &ioInfo, NULL);
   if(readRes < 0)
   { // error
      FhgfsOpsHelper_closefileWithAsyncRetry(entryInfo, &ioInfo, NULL);

      retVal = FhgfsOpsErr_toSysErr(-readRes);
      goto clean_up_open;
   }

   retVal = readRes;

   // close the file

   closeRes = FhgfsOpsHelper_closefileWithAsyncRetry(entryInfo, &ioInfo, NULL);

   if(closeRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(closeRes);
   }


   // clean-up

clean_up_open:
   if (ioInfo.firstWriteDone)
      BitStore_uninit(ioInfo.firstWriteDone);

   RemotingIOInfo_freeVals(&ioInfo);

   return retVal;
}

/**
 * Opens a file (the file must exist), writes the data to it and closes it.
 *
 * Note: This is really slow, because of the open/close overhead - use _writeStatelessInode()
 * method instead, if possible.
 *
 * @return number of written bytes or negative linux error code
 */
ssize_t FhgfsOpsHelper_writeStateless(App* app, const EntryInfo* entryInfo,
   const char __user *buf, size_t size, loff_t offset, unsigned uid, unsigned gid)
{
   int retVal = -EREMOTEIO;

   AtomicInt maxUsedTargetIndex;
   RemotingIOInfo ioInfo;
   FhgfsOpsErr openRes;
   ssize_t writeRes;
   FhgfsOpsErr closeRes;
   BitStore firstWriteDone;
   size_t numTargets;
   PathInfo pathInfo;

   AtomicInt_init(&maxUsedTargetIndex, -1);

   // open file

   memset(&pathInfo, 0, sizeof(pathInfo) );

   RemotingIOInfo_initOpen(app, OPENFILE_ACCESS_WRITE, &maxUsedTargetIndex, &pathInfo, &ioInfo);

   openRes = FhgfsOpsRemoting_openfile(entryInfo, &ioInfo, NULL, NULL);

   if(openRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(openRes);
      goto clean_up_open;
   }

   numTargets = UInt16Vec_length(ioInfo.pattern->getStripeTargetIDs(ioInfo.pattern));
   BitStore_initWithSizeAndReset(&firstWriteDone, numTargets);
   ioInfo.firstWriteDone = &firstWriteDone;
   ioInfo.userID = uid;
   ioInfo.groupID = gid;

   // write file

   writeRes = FhgfsOpsRemoting_writefile(buf, size, offset, &ioInfo);
   if(writeRes < 0)
   { // error
      FhgfsOpsHelper_closefileWithAsyncRetry(entryInfo, &ioInfo, NULL);

      retVal = FhgfsOpsErr_toSysErr(-writeRes);
      goto clean_up_open;
   }

   retVal = writeRes;

   // close file

   closeRes = FhgfsOpsHelper_closefileWithAsyncRetry(entryInfo, &ioInfo, NULL);

   if(closeRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(closeRes);
   }


   // clean-up

clean_up_open:
   if (ioInfo.firstWriteDone)
      BitStore_uninit(ioInfo.firstWriteDone);

   RemotingIOInfo_freeVals(&ioInfo);

   return retVal;
}

/**
 * Writes the given buffer by getting a reference handle from the inode and releasing that handle
 * immediately after the write.
 *
 * Note: This can safe the overhead for remote open/close if the file is already open for writing
 * (or reading+writing) by any other process, so it is way better than the _writeStateless()
 * method.
 *
 * @param offset offset in file, -1 for append
 * @return bytes written or negative fhgfs error code
 */
ssize_t FhgfsOpsHelper_writeStatelessInode(FhgfsInode* fhgfsInode, const char __user *buf,
   size_t size, loff_t offset)
{
   FileHandleType handleType;
   RemotingIOInfo ioInfo;

   FhgfsOpsErr referenceRes;
   ssize_t writeRes;
   FhgfsOpsErr releaseRes;


   // open file

   /* referenceHandle needs a dentry only for possible TRUNC operations. */
   referenceRes = FhgfsInode_referenceHandle(fhgfsInode, NULL, OPENFILE_ACCESS_WRITE, true, NULL,
         &handleType, NULL);
   if(unlikely(referenceRes != FhgfsOpsErr_SUCCESS) )
   { // error
      return -referenceRes;
   }

   // write file

   FhgfsInode_getRefIOInfo(fhgfsInode, handleType, FhgfsInode_handleTypeToOpenFlags(handleType),
      &ioInfo);

   writeRes = FhgfsOpsHelper_writefileEx(fhgfsInode, buf, size, offset, &ioInfo);
   if(unlikely(writeRes < 0) )
   { // error
      FhgfsInode_releaseHandle(fhgfsInode, handleType, NULL);

      return writeRes;
   }

   // close file

   releaseRes = FhgfsInode_releaseHandle(fhgfsInode, handleType, NULL);
   if(unlikely(releaseRes != FhgfsOpsErr_SUCCESS) )
   { // error
      return -releaseRes;
   }

   return writeRes;
}
