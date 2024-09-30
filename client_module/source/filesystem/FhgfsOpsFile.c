#include <app/log/Logger.h>
#include <app/App.h>
#include <app/config/Config.h>
#include <common/toolkit/vector/StrCpyVec.h>
#include <common/toolkit/LockingTk.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/StringTk.h>
#include <filesystem/ProcFs.h>
#include <os/iov_iter.h>
#include <os/OsCompat.h>
#include <os/OsTypeConversion.h>
#include "FhgfsOpsHelper.h"
#include "FhgfsOpsFile.h"
#include "FhgfsOpsDir.h"
#include "FhgfsOpsInode.h"
#include "FhgfsOpsIoctl.h"
#include "FhgfsOpsSuper.h"
#include "FhgfsOps_versions.h"
#include "FhgfsOpsPages.h"

#include <linux/aio.h>
#include <linux/writeback.h>
#include <linux/mm.h>
#include <linux/mpage.h>
#include <linux/backing-dev.h>
#include <linux/pagemap.h>
#include <linux/delay.h>


#ifdef CONFIG_COMPAT
#include <asm/compat.h>
#endif // CONFIG_COMPAT

static ssize_t FhgfsOps_buffered_write_iter(struct kiocb *iocb, struct iov_iter *from);
static ssize_t FhgfsOps_buffered_read_iter(struct kiocb *iocb, struct iov_iter *to);

#ifdef KERNEL_WRITE_BEGIN_HAS_FLAGS
int FhgfsOps_write_begin(struct file* file, struct address_space* mapping,
      loff_t pos, unsigned len, unsigned flags, struct page** pagep, void** fsdata);
#else
int FhgfsOps_write_begin(struct file* file, struct address_space* mapping,
      loff_t pos, unsigned len, struct page** pagep, void** fsdata);
#endif

int FhgfsOps_write_end(struct file* file, struct address_space* mapping,
      loff_t pos, unsigned len, unsigned copied, struct page* page, void* fsdata);

#define MMAP_RETRY_LOCK_EASY 100
#define MMAP_RETRY_LOCK_HARD 500

/**
 * Operations for files with cache type "buffered" and "none".
 */
struct file_operations fhgfs_file_buffered_ops =
{
   .open             = FhgfsOps_open,
   .release          = FhgfsOps_release,
   .fsync            = FhgfsOps_fsync,
   .flush            = FhgfsOps_flush,
   .llseek           = FhgfsOps_llseek,
   .flock            = FhgfsOps_flock,
   .lock             = FhgfsOps_lock,
   .mmap             = FhgfsOps_mmap,
   .unlocked_ioctl   = FhgfsOpsIoctl_ioctl,
#ifdef CONFIG_COMPAT
   .compat_ioctl     = FhgfsOpsIoctl_compatIoctl,
#endif // CONFIG_COMPAT
#ifdef KERNEL_HAS_GENERIC_FILE_SPLICE_READ
    .splice_read  = generic_file_splice_read,
#else
    .splice_read  = filemap_splice_read,
#endif
#ifdef KERNEL_HAS_ITER_FILE_SPLICE_WRITE
    .splice_write = iter_file_splice_write,
#else
    .splice_write = generic_file_splice_write,
#endif

   .read_iter           = FhgfsOps_buffered_read_iter,
   .write_iter          = FhgfsOps_buffered_write_iter, // replacement for aio_write

#ifdef KERNEL_HAS_GENERIC_FILE_SENDFILE
   .sendfile   = generic_file_sendfile, // removed in 2.6.23 (now handled via splice)
#endif // LINUX_VERSION_CODE
};

/**
 * Operations for files with cache type "paged".
 */
struct file_operations fhgfs_file_pagecache_ops =
{
   .open                = FhgfsOps_open,
   .release             = FhgfsOps_release,
   .read_iter           = FhgfsOps_read_iter,
   .write_iter          = FhgfsOps_write_iter,
   .fsync               = FhgfsOps_fsync,
   .flush               = FhgfsOps_flush,
   .llseek              = FhgfsOps_llseek,
   .flock               = FhgfsOps_flock,
   .lock                = FhgfsOps_lock,
   .mmap                = FhgfsOps_mmap,
   .unlocked_ioctl      = FhgfsOpsIoctl_ioctl,
#ifdef CONFIG_COMPAT
   .compat_ioctl        = FhgfsOpsIoctl_compatIoctl,
#endif // CONFIG_COMPAT
#ifdef KERNEL_HAS_GENERIC_FILE_SPLICE_READ
    .splice_read  = generic_file_splice_read,
#else
    .splice_read  = filemap_splice_read,
#endif
#ifdef KERNEL_HAS_ITER_FILE_SPLICE_WRITE
    .splice_write = iter_file_splice_write,
#else
    .splice_write = generic_file_splice_write,
#endif

#ifdef KERNEL_HAS_GENERIC_FILE_SENDFILE
   .sendfile   = generic_file_sendfile, // removed in 2.6.23 (now handled via splice)
#endif // LINUX_VERSION_CODE
};

struct file_operations fhgfs_dir_ops =
{
   .open             = FhgfsOps_opendirIncremental,
   .release          = FhgfsOps_releasedir,
#ifdef KERNEL_HAS_ITERATE_DIR
#if defined(KERNEL_HAS_FOPS_ITERATE)
   .iterate       = FhgfsOps_iterateIncremental, // linux 3.11 renamed readdir to iterate
#else
   .iterate_shared   = FhgfsOps_iterateIncremental, // linux 6.3 removed .iterate & it's a parallel variant of .iterate().
#endif
#else
   .readdir       = FhgfsOps_readdirIncremental, // linux 3.11 renamed readdir to iterate
#endif // LINUX_VERSION_CODE
   .read             = generic_read_dir, // just returns the appropriate error code
   .fsync            = FhgfsOps_fsync,
   .llseek           = FhgfsOps_llseekdir,
   .unlocked_ioctl   = FhgfsOpsIoctl_ioctl,
#ifdef CONFIG_COMPAT
   .compat_ioctl     = FhgfsOpsIoctl_compatIoctl,
#endif // CONFIG_COMPAT
};

/**
 * Operations for files with cache type "buffered" and "none".
 */
struct address_space_operations fhgfs_address_ops =
{
#ifdef KERNEL_HAS_READ_FOLIO
   .read_folio     = FhgfsOps_read_folio,
#else
   .readpage       = FhgfsOpsPages_readpage,
#endif

#ifdef KERNEL_HAS_FOLIO
   .readahead      = FhgfsOpsPages_readahead,
   .dirty_folio    = filemap_dirty_folio,
#else
   .readpages      = FhgfsOpsPages_readpages,
   .set_page_dirty = __set_page_dirty_nobuffers,
#endif
   .writepage      = FhgfsOpsPages_writepage,
   .writepages     = FhgfsOpsPages_writepages,
   .direct_IO      = FhgfsOps_directIO,
   .write_begin   = FhgfsOps_write_begin,
   .write_end     = FhgfsOps_write_end,
};

/**
 * Operations for files with cache type "paged".
 */
struct address_space_operations fhgfs_address_pagecache_ops =
{
#ifdef KERNEL_HAS_READ_FOLIO
   .read_folio     = FhgfsOps_read_folio,
#else
   .readpage       = FhgfsOpsPages_readpage,
#endif

#ifdef KERNEL_HAS_FOLIO
   .readahead      = FhgfsOpsPages_readahead,
   .dirty_folio    = filemap_dirty_folio,
#else
   .readpages      = FhgfsOpsPages_readpages,
   .set_page_dirty = __set_page_dirty_nobuffers,
#endif
   .writepage      = FhgfsOpsPages_writepage,
   .writepages     = FhgfsOpsPages_writepages,
   .direct_IO      = FhgfsOps_directIO,
   .write_begin   = FhgfsOps_write_begin,
   .write_end     = FhgfsOps_write_end,
};


/**
 * note: rewinddir is seek to offset 0.
 *
 * @param origin dirs allow only SEEK_SET (via seekdir/rewinddir from userspace).
 */
loff_t FhgfsOps_llseekdir(struct file *file, loff_t offset, int origin)
{
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   const char* logContext = "FhgfsOps_llseekDir";
   struct inode* inode = file_inode(file);

   loff_t retVal = 0;
   FsDirInfo* dirInfo = __FhgfsOps_getDirInfo(file);

   FhgfsOpsHelper_logOpMsg(Log_SPAM, app, file_dentry(file), inode, logContext,
      "offset: %lld directive: %d", (long long)offset, origin);

   if(origin != SEEK_SET)
   {
      if (origin == SEEK_CUR && offset == 0) {
         // Some applications use lseek with SEEK_CUR and offset = 0 to get the current position in
         // the file. To support that special case, we will translate the request into a SEEK_SET
         // with the current file position as the offset.
         offset = file->f_pos;
         origin = SEEK_SET;
         FhgfsOpsHelper_logOpMsg(Log_SPAM, app, file_dentry(file), inode, logContext,
            "offset: %lld position: %lld directive: %d", (long long)offset, (long long)file->f_pos,
            origin);
      } else {
         return -EINVAL;
      }
   }


   retVal = generic_file_llseek_unlocked(file, offset, origin);
   if(likely(retVal >= 0) )
   {
      // invalidate any retrieved contents to keep things in sync with server offset
      StrCpyVec* contents = FsDirInfo_getDirContents(dirInfo);

      StrCpyVec_clear(contents);
      FsDirInfo_setCurrentContentsPos(dirInfo, 0);

      FsDirInfo_setServerOffset(dirInfo, offset);
      FsDirInfo_setEndOfDir(dirInfo, false);
   }

   return retVal;
}

loff_t FhgfsOps_llseek(struct file *file, loff_t offset, int origin)
{
   const char* logContext = "FhgfsOps_llseek";
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);

   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   bool isGloballyLockedAppend =
      FsFileInfo_getAppending(fileInfo) && Config_getTuneUseGlobalAppendLocks(cfg);

   loff_t retVal = 0;
   struct inode *inode = file->f_mapping->host;

   FhgfsIsizeHints iSizeHints;

   if(unlikely(Logger_getLogLevel(log) >= Log_SPAM) )
      FhgfsOpsHelper_logOpMsg(Log_SPAM, app, file_dentry(file), inode, logContext,
         "offset: %lld directive: %d", (long long)offset, origin);

   /* note: globally locked append with SEEK_CUR is a special case, because we need to flush
      the cache to find out the current offset (which is not required without append) */
   if( (origin == SEEK_END) ||
       (isGloballyLockedAppend && (origin == SEEK_CUR) ) )
   { // seek to position relative to end-of-file => flush cache and update current file size first

      // (note: refreshInode() also flushes caches for correct file size)

      retVal = __FhgfsOps_refreshInode(app, inode, NULL, &iSizeHints);
      if(retVal)
         goto clean_up;

      spin_lock(&inode->i_lock); // L O C K

      // SEEK_CUR reads (and modifies) f_pos, so in buffered append mode move to end first
      if(origin == SEEK_CUR)
         file->f_pos = inode->i_size;

      retVal = generic_file_llseek_unlocked(file, offset, origin);

      spin_unlock(&inode->i_lock); // U N L O C K
   }
   else
   { // abolute or relative-to-current_pos seeks => generic stuff
      retVal = generic_file_llseek_unlocked(file, offset, origin);
   }


clean_up:
   // clean-up

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, logContext, "retVal: %lld",
      retVal);

   return retVal;
}

/**
 * Note: Currently unsused method, as we're using the kernel's generic_readlink function.
 */
int FhgfsOps_readlink(struct dentry* dentry, char __user* buf, int size)
{
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOps_readlink";

   int retVal;
   struct inode* inode = dentry->d_inode;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   if(unlikely(Logger_getLogLevel(log) >= 5) )
      FhgfsOpsHelper_logOp(5, app, dentry, inode, logContext);

   // check user buffer
   if(unlikely(!os_access_ok(VERIFY_WRITE, buf, size) ) )
      return -EFAULT;

   FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo

   retVal = FhgfsOpsHelper_readlink_kernel(app, FhgfsInode_getEntryInfo(fhgfsInode), buf, size);

   FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo


   return retVal;
}

/**
 * Opens a directory and prepares the handle for incremental readdir().
 */
int FhgfsOps_opendirIncremental(struct inode* inode, struct file* file)
{
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOps_opendirIncremental";

   int retVal = 0;
   //struct dentry* dentry = file_dentry(file);
   FsDirInfo* dirInfo;

   if(unlikely(Logger_getLogLevel(log) >= Log_SPAM) )
      FhgfsOpsHelper_logOp(Log_SPAM, app, file_dentry(file), inode, logContext);

   //retVal = __FhgfsOps_refreshInode(app, inode); // not necessary
   if(!retVal)
   { // success
      dirInfo = FsDirInfo_construct(app);
      __FhgfsOps_setDirInfo(dirInfo, file);
   }

#ifdef FMODE_KABI_ITERATE
   file->f_mode |= FMODE_KABI_ITERATE;
#endif

   return retVal;
}

#ifdef KERNEL_HAS_ITERATE_DIR
int FhgfsOps_iterateIncremental(struct file* file, struct dir_context* ctx)
#else
int FhgfsOps_readdirIncremental(struct file* file, void* buf, filldir_t filldir)
#endif // LINUX_VERSION_CODE
{
   /* note: if the user seeks to a custom offset, llseekdir will invalidate any retrieved contents
      and set the new offset in the dirinfo object */

   struct dentry* dentry = file_dentry(file);
   struct super_block* superBlock = dentry->d_sb;
   App* app = FhgfsOps_getApp(superBlock);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOps_readdirIncremental";

   int retVal = 0;
   FsDirInfo* dirInfo = __FhgfsOps_getDirInfo(file);
   struct inode* inode = file_inode(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   StrCpyVec* dirContents = FsDirInfo_getDirContents(dirInfo);
   UInt8Vec* dirContentsTypes = FsDirInfo_getDirContentsTypes(dirInfo);
   StrCpyVec* dirContentIDs = FsDirInfo_getEntryIDs(dirInfo);
   Int64CpyVec* serverOffsets = FsDirInfo_getServerOffsets(dirInfo);

   #ifdef KERNEL_HAS_ITERATE_DIR
      loff_t* pos = &(ctx->pos); // used by dir_emit()
   #else
      loff_t* pos = &(file->f_pos);
   #endif // LINUX_VERSION_CODE


   if(unlikely(Logger_getLogLevel(log) >= Log_SPAM) )
      FhgfsOpsHelper_logOp(Log_SPAM, app, dentry, inode, logContext);


   FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo


   // loop as long as filldir (or dir_emit) swallows more entries (or end of dir contents reached)
   for( ; ; )
   {
      int refreshRes;
      size_t contentsPos;
      size_t contentsLength;
      char* currentName;
      DirEntryType currentEntryType;
      unsigned currentOSEntryType;
      uint64_t currentIno;

      refreshRes = FhgfsOpsHelper_refreshDirInfoIncremental(app,
         FhgfsInode_getEntryInfo(fhgfsInode), dirInfo, false);
      if(unlikely(refreshRes) )
      { // error occurred
         retVal = refreshRes;
         break;
      }

      contentsLength = StrCpyVec_length(dirContents);

      /* refreshDirInfoIncremental() guarantees that we either have a valid range for current
         dir offset or that dirContents list is empty */
      if(!contentsLength)
      { // end of dir
         LOG_DEBUG(log, Log_SPAM, logContext, "reached end of dir");
         break;
      }

      contentsPos = FsDirInfo_getCurrentContentsPos(dirInfo);

      currentName = StrCpyVec_at(dirContents, contentsPos);

      currentEntryType = UInt8Vec_at(dirContentsTypes, contentsPos);
      currentOSEntryType = OsTypeConv_dirEntryTypeToOS(currentEntryType);


      LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext,
         "name: %s; pos: %lld; contentsPos: %lld/%lld; finalContents: %s",
         currentName, (long long)*pos, (long long)contentsPos,
         (long long)contentsLength, FsDirInfo_getEndOfDir(dirInfo) ? "yes" : "no");


      if(!strcmp(".", currentName) )
         currentIno = inode->i_ino;
      else
      if(!strcmp("..", currentName) )
         currentIno = parent_ino(dentry);
      else
      { // generate inode number from entryID
         const char* currentEntryID = StrCpyVec_at(dirContentIDs, contentsPos);

         currentIno = FhgfsInode_generateInodeID(superBlock, currentEntryID,
            strlen(currentEntryID) );
      }


      if(is_32bit_api() && (currentIno > UINT_MAX) )
         currentIno = currentIno >> 32; // (32-bit apps would fail with EOVERFLOW)


      #ifdef KERNEL_HAS_ITERATE_DIR
         if(!dir_emit(
            ctx, currentName, strlen(currentName), currentIno, currentOSEntryType) )
            break;
      #else
         if(filldir(
            buf, currentName, strlen(currentName), *pos, currentIno, currentOSEntryType) < 0)
            break;
      #endif // LINUX_VERSION_CODE


      LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext, "filled: %s", currentName);

      // advance dir position (yes, it's alright to use the old contentsPos for the next round here)
      (*pos) = Int64CpyVec_at(serverOffsets, contentsPos);

      // increment contents vector offset
      FsDirInfo_setCurrentContentsPos(dirInfo, contentsPos+1);

   } // end of for-loop


   // clean-up
   FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo

   return retVal;
}


/**
 * Note: This works for _opendir() and for _opendirIncremental().
 */
int FhgfsOps_releasedir(struct inode* inode, struct file* file)
{
   const char* logContext = "FhgfsOps_releasedir";

   FsObjectInfo* fsObjectInfo = __FhgfsOps_getObjectInfo(file);

   App* app = FsObjectInfo_getApp(fsObjectInfo);

   FhgfsOpsHelper_logOp(Log_SPAM, app, file_dentry(file), inode, logContext);

   FsObjectInfo_virtualDestruct(fsObjectInfo);

   return 0;
}

/**
 * Open a file, may be called from vfs or lookup/atomic open.
 *
 * @param lookupInfo is NULL if this is a direct open call from the vfs
 */
int FhgfsOps_openReferenceHandle(App* app, struct inode* inode, struct file* file,
   unsigned openFlags, LookupIntentInfoOut* lookupInfo, uint64_t* outVersion)
{
   Config* cfg = App_getConfig(app);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOps_openReferenceHandle";

   struct super_block* sb = inode->i_sb;
   struct dentry* dentry = file_dentry(file);

   int retVal = 0;
   int fhgfsOpenFlags;
   FileHandleType handleType;
   FhgfsOpsErr openRes;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   if(unlikely(Logger_getLogLevel(log) >= 4) )
      FhgfsOpsHelper_logOp(Log_DEBUG, app, dentry, inode, logContext);

   fhgfsOpenFlags = OsTypeConv_openFlagsOsToFhgfs(openFlags, __FhgfsOps_isPagedMode(sb) );

   openRes = FhgfsInode_referenceHandle(fhgfsInode, file_dentry(file), fhgfsOpenFlags, false,
      lookupInfo, &handleType, outVersion);

   LOG_DEBUG_FORMATTED(log, 4, logContext, "remoting complete. result: %s",
      FhgfsOpsErr_toErrString(openRes) );

   if(openRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(openRes);
   }
   else
   { // success => file is open (=> handle open flags)
      FsFileInfo* fileInfo = FsFileInfo_construct(app, fhgfsOpenFlags, handleType);

      // handle O_APPEND
      if(file->f_flags & O_APPEND)
         FsFileInfo_setAppending(fileInfo, true);

      // handle O_DIRECT + disabled caching
      if( (file->f_flags & O_DIRECT) ||
          ( (file->f_flags & O_APPEND) && !Config_getTuneUseBufferedAppend(cfg) ) ||
          (Config_getTuneFileCacheTypeNum(cfg) == FILECACHETYPE_None) )
      { // disable caching
         FsFileInfo_setAllowCaching(fileInfo, false);
      }

      __FhgfsOps_setFileInfo(fileInfo, file);
   }

   return retVal;
}

/**
 * Open a file, vfs interface
 */
int FhgfsOps_open(struct inode* inode, struct file* file)
{
   const char* logContext = "FhgfsOps_open";

   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   Logger* log = App_getLogger(app);

   struct dentry* dentry = file_dentry(file);

   unsigned openFlags = file->f_flags;
   LookupIntentInfoOut* lookupInfo = NULL; // not available for direct open

   if(unlikely(Logger_getLogLevel(log) >= 4) )
      FhgfsOpsHelper_logOp(4, app, dentry, inode, logContext);

   return FhgfsOps_openReferenceHandle(app, inode, file, openFlags, lookupInfo, NULL);
}

/**
 * Close a file.
 *
 * Note: We only got one shot, even in case of an error.
 */
int FhgfsOps_release(struct inode* inode, struct file* file)
{
   const char* logContext = "FhgfsOps_release";

   int retVal = 0;
   FhgfsOpsErr closeRes;
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   FsObjectInfo* fsObjectInfo = __FhgfsOps_getObjectInfo(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   FileHandleType handleType = FsFileInfo_getHandleType(fileInfo);

   App* app = FsObjectInfo_getApp(fsObjectInfo);

   FhgfsOpsHelper_logOp(Log_SPAM, app, file_dentry(file), inode, logContext);

   if(unlikely(!fileInfo) )
   { // invalid file handle
      return -EBADF;
   }

   FhgfsOps_releaseCancelLocks(inode, file); // cancel all locks that were not properly released yet

   closeRes = FhgfsInode_releaseHandle(fhgfsInode, handleType, file_dentry(file));

   if(closeRes != FhgfsOpsErr_SUCCESS)
   { // error
      retVal = FhgfsOpsErr_toSysErr(closeRes);
   }

   // note: we free the fileInfo no matter whether the communication succeeded or not
   //    (because _release() won't be called again even if it didn't succeed)

   FsObjectInfo_virtualDestruct( (FsObjectInfo*)fileInfo);
   __FhgfsOps_setFileInfo( (FsFileInfo*)NULL, file);

   FhgfsInode_invalidateCache(fhgfsInode);

   // warning: linux vfs won't return this result to user apps. only flush() res is passed to apps.
   return retVal;
}

/**
 * Called during file close to unlock remaining entry locks and range locks that were not properly
 * unlocked by the user-space application yet.
 */
int FhgfsOps_releaseCancelLocks(struct inode* inode, struct file* file)
{
   int retVal = 0;
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   RemotingIOInfo ioInfo;
   FhgfsOpsErr unlockRes;

   /* (note: it is very unlikely that an application will use entry and range locking together on
      the same file, so we have no special optimization regarding EntryMinInfoCopy for that case) */

   if(FsFileInfo_getUsedEntryLocking(fileInfo) )
   { // entry locks were used with this file handle
      int64_t clientFD = __FhgfsOps_getCurrentLockFD(file);

      FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

      FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo

      unlockRes = FhgfsOpsHelper_unlockEntryWithAsyncRetry(&fhgfsInode->entryInfo,
         &fhgfsInode->entryInfoLock, &ioInfo, clientFD);

      FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo

      if(!retVal)
         retVal = FhgfsOpsErr_toSysErr(unlockRes);
   }

   /* (note: FhgfsInode_getNumRangeLockPIDs() below is a shortcut to save the time for mutex locking
      if no range locks were used for this inode.) */

   if(FhgfsInode_getNumRangeLockPIDs(fhgfsInode) &&
      FhgfsInode_removeRangeLockPID(fhgfsInode, __FhgfsOps_getCurrentLockPID() ) )
   { // current pid used range locking on this inode
      int ownerPID = __FhgfsOps_getCurrentLockPID();

      FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

      FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo

      unlockRes = FhgfsOpsHelper_unlockRangeWithAsyncRetry(&fhgfsInode->entryInfo,
         &fhgfsInode->entryInfoLock, &ioInfo, ownerPID);

      FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo

      if(!retVal)
         retVal = FhgfsOpsErr_toSysErr(unlockRes);
   }

   return retVal;
}

/**
 * Called by flock syscall.
 *
 * @return 0 on success, negative linux error code otherwise
 */
int FhgfsOps_flock(struct file* file, int cmd, struct file_lock* fileLock)
{
   const char* logContext = __func__;

   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);

   struct inode* inode = file_inode(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   bool useGlobalFileLocks = Config_getTuneUseGlobalFileLocks(cfg);
   FhgfsOpsErr globalLockRes = FhgfsOpsErr_SUCCESS;
   int lockTypeFlags;

   lockTypeFlags = OsTypeConv_flockTypeToFhgfs(fileLock);

   if(unlikely(Logger_getLogLevel(log) >= Log_SPAM) )
      FhgfsOpsHelper_logOpMsg(Log_SPAM, app, file_dentry(file), inode, logContext, "lockType: %s",
         LockingTk_lockTypeToStr(lockTypeFlags) );


   // flush buffers before removing a global lock

   if(useGlobalFileLocks && (lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_REMOVE) )
   {
      int flushRes = __FhgfsOps_flush(app, file, false, false, true,
         false);

      /* note: can't return error here and must continue, because local unlock must always be done
         to avoid BUG() statement being triggered in locks_remove_flock() on cleanup after kill */
      if(unlikely(flushRes < 0) )
         Logger_logFormatted(log, Log_NOTICE, logContext,
            "Flushing before unlock failed. Continuing anyways. flushRes: %d", flushRes);
   }

   // global locking

   if(useGlobalFileLocks)
   {
      FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
      RemotingIOInfo ioInfo;

      FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

      FsFileInfo_setUsedEntryLocking(fileInfo);

      FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo

      globalLockRes = FhgfsOpsRemoting_flockEntryEx(&fhgfsInode->entryInfo,
         &fhgfsInode->entryInfoLock, app, ioInfo.fileHandleID, (size_t)fileLock->fl_file,
         fileLock->fl_pid, lockTypeFlags, true);

      FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo

      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "remoting complete. result: %s",
         FhgfsOpsErr_toErrString(globalLockRes) );

      /* note: local unlock must always be done for cleanup (otherwise e.g. killing a process
         holding a lock results in the BUG() statement being triggered in  locks_remove_flock() ) */
      if( (globalLockRes != FhgfsOpsErr_SUCCESS) &&
         !(lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_REMOVE) )
         return FhgfsOpsErr_toSysErr(globalLockRes);
   }

   // local locking

   {
      #if defined(KERNEL_HAS_LOCKS_FILELOCK_INODE_WAIT) || defined(KERNEL_HAS_LOCKS_LOCK_INODE_WAIT)
         int localLockRes = locks_lock_inode_wait(file_inode(file), fileLock);
      #else
         int localLockRes = flock_lock_file_wait(file, fileLock);
      #endif

      if(!useGlobalFileLocks)
         return localLockRes;

      if(localLockRes &&
         (globalLockRes == FhgfsOpsErr_SUCCESS) &&
         (lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_ADD) )
      { // sanity check
         Logger_logFormatted(log, Log_NOTICE, logContext,
            "Unexpected: Global locking succeeded, but local locking failed. SysErr: %d",
            localLockRes);
      }
   }

   // flush buffers after we got a new global lock

   if(useGlobalFileLocks &&
      (lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_ADD) &&
      (globalLockRes == FhgfsOpsErr_SUCCESS) )
   {
      int flushRes = __FhgfsOps_flush(app, file, false, false, true,
         false);

      if(unlikely(flushRes < 0) )
         return flushRes; // flush error occured
   }

   return FhgfsOpsErr_toSysErr(globalLockRes);
}

/**
 * Called by fcntl syscall (F_GETLK, F_SETLK, F_SETLKW) for file range locking.
 *
 * @return 0 on success, negative linux error code otherwise
 */
int FhgfsOps_lock(struct file* file, int cmd, struct file_lock* fileLock)
{
   const char* logContext = "FhgfsOps_lock (fcntl)";

   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);

   struct inode* inode = file_inode(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   bool useGlobalFileLocks = Config_getTuneUseGlobalFileLocks(cfg);
   FhgfsOpsErr globalLockRes = FhgfsOpsErr_SUCCESS;
   int lockTypeFlags;


   // handle user request for conflicting locks (always local-only currently, see notes below)

   if(cmd == F_GETLK)
   { // get confliciting lock

      /* note: it's questionable if returning remote locks makes sense (because the local app could
         misinterpret the pid of the lock-holder), so we do local only for now. */

      posix_test_lock(file, fileLock);

      /* note: "fileLock->fl_type != F_UNLCK" would tell us now whether a conflicting local lock
         was found */

      return 0;
   }


   lockTypeFlags = OsTypeConv_flockTypeToFhgfs(fileLock);

   if(unlikely(Logger_getLogLevel(log) >= Log_SPAM) )
      FhgfsOpsHelper_logOpMsg(Log_SPAM, app, file_dentry(file), inode, logContext,
         "lockType: %s; start: %lld; end: %lld", LockingTk_lockTypeToStr(lockTypeFlags),
         (long long)fileLock->fl_start, (long long)fileLock->fl_end);


   // flush buffers before removing a global lock

   if(useGlobalFileLocks &&
      (lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_REMOVE) )
   {
      int flushRes = __FhgfsOps_flush(app, file, false, false, true, false);

      /* note: can't return error here and must continue, because local unlock must always be done
         to avoid BUG() statement being triggered in locks_remove_flock() on cleanup after kill */
      if(unlikely(flushRes < 0) )
         Logger_logFormatted(log, Log_NOTICE, logContext,
            "Flushing before unlock failed. Continuing anyways. flushRes: %d", flushRes);
   }

   // global locking

   if(useGlobalFileLocks)
   {
      FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
      RemotingIOInfo ioInfo;

      FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

      FhgfsInode_addRangeLockPID(fhgfsInode, fileLock->fl_pid);

      FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo

      globalLockRes = FhgfsOpsRemoting_flockRangeEx(&fhgfsInode->entryInfo,
         &fhgfsInode->entryInfoLock, ioInfo.app, ioInfo.fileHandleID, fileLock->fl_pid,
         lockTypeFlags, fileLock->fl_start, fileLock->fl_end, true);

      FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo

      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "remoting complete. result: %s",
         FhgfsOpsErr_toErrString(globalLockRes) );

      /* note: local unlock must always be done for cleanup (otherwise e.g. killing a process
         holding a lock results in the BUG() statement being triggered in  locks_remove_flock() ) */
      if( (globalLockRes != FhgfsOpsErr_SUCCESS) &&
         !(lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_REMOVE) )
         return FhgfsOpsErr_toSysErr(globalLockRes);
   }

   // local locking

   {
      /* note on local+global locking:
         we need to call posix_lock_file_wait() even with global locks, because inode->i_flock needs
         to be set, so that locks_remove_posix() gets active (via filp_close() ) and thus the
         condition "Record locks are not inherited by a child created via fork(2), but are preserved
         across an execve(2)." [man 2 fcntl] holds.
         Otherwise we wouldn't be notified about an unlock on parent process exit, as there are
         still references to the filp and thus our ->release() isn't invoked. (See trac #271) */

      /* note on local/global locking order:
         local locking needs to be done after global locking, because otherwise if global locking
         failed we wouldn't know how to undo the local locking (e.g. if the process acquires a
         shared lock for the second time or does a merge with existing ranges). */

#if defined(KERNEL_HAS_LOCKS_FILELOCK_INODE_WAIT) || defined(KERNEL_HAS_LOCKS_LOCK_INODE_WAIT)
      int localLockRes = locks_lock_inode_wait(file_inode(file), fileLock);
#else
      int localLockRes = posix_lock_file_wait(file, fileLock);
#endif

      //printk_fhgfs_debug(KERN_WARNING, "posix_lock_file result=%d, cmd=%d\n", localLockRes, cmd);

      if(!useGlobalFileLocks)
         return localLockRes;

      if(localLockRes &&
         (globalLockRes == FhgfsOpsErr_SUCCESS) &&
         (lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_ADD) )
      { // sanity check
         Logger_logFormatted(log, Log_NOTICE, logContext,
            "Unexpected: Global locking succeeded, but local locking failed. SysErr: %d",
            localLockRes);
      }
   }

   // flush buffers after we got a new global lock

   if(useGlobalFileLocks &&
      (lockTypeFlags & ENTRYLOCKTYPE_LOCKOPS_ADD) &&
      (globalLockRes == FhgfsOpsErr_SUCCESS) )
   {
      int flushRes = __FhgfsOps_flush(app, file, false, false, true, false);
      if(unlikely(flushRes < 0) )
         return flushRes; // flush error occured
   }

   return FhgfsOpsErr_toSysErr(globalLockRes);
}


static ssize_t read_common(struct file *file, struct iov_iter *iter, size_t size, loff_t *offsetPointer)
{
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);

   struct inode* inode = file->f_mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   RemotingIOInfo ioInfo;
   ssize_t readRes;

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, __func__, "(offset: %lld; size: %lld)",
      (long long)*offsetPointer, (long long)size);
   IGNORE_UNUSED_VARIABLE(app);

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   if (app->cfg->tuneCoherentBuffers)
   {
      readRes = filemap_write_and_wait(file->f_mapping);
      if (readRes < 0)
         return readRes;

      // ignore the -EBUSY we could receive here, because there is just *no* way we can keep caches
      // coherent without locking everything all the time. if this produces inconsistent data,
      // something must have been racy anyway.
      invalidate_inode_pages2(file->f_mapping);
      // Increment coherent read/write counter
      atomic_inc(&fhgfsInode->coRWInProg);
   }

   readRes = FhgfsOpsHelper_readCached(iter, size, *offsetPointer, fhgfsInode, fileInfo, &ioInfo);
   //readRes = FhgfsOpsRemoting_readfile(buf, size, *offsetPointer, &ioInfo);

   if(readRes < 0)
   { // read error (=> transform negative fhgfs error code to system error code)
      if (app->cfg->tuneCoherentBuffers)
         atomic_dec(&fhgfsInode->coRWInProg);
      return FhgfsOpsErr_toSysErr(-readRes);
   }

   *offsetPointer += readRes;
   FsFileInfo_setLastReadOffset(fileInfo, *offsetPointer);

   if( ( (size_t)readRes < size) && (i_size_read(inode) > *offsetPointer) )
   { // sparse file compatibility mode
      ssize_t readSparseRes = __FhgfsOps_readSparse(
         file, iter, size - readRes, *offsetPointer);

      if(unlikely(readSparseRes < 0) )
      {
         if (app->cfg->tuneCoherentBuffers)
            atomic_dec(&fhgfsInode->coRWInProg);
         return readSparseRes;
      }

      *offsetPointer += readSparseRes;
      readRes += readSparseRes;

      FsFileInfo_setLastReadOffset(fileInfo, *offsetPointer);
   }

   // add to /proc/<pid>/io
   task_io_account_read(readRes);

   // Decrement coherent read/write counter
   if (app->cfg->tuneCoherentBuffers)
      atomic_dec(&fhgfsInode->coRWInProg);
 
   return readRes;
}


/**
 * Special reading mode that is slower (e.g. not parallel) but compatible with sparse files.
 *
 * Note: Intended to be just a helper for actual read methods (e.g. won't increase the offset
 * pointer).
 *
 * @return negative Linux error code on error, read bytes otherwise
 */
ssize_t __FhgfsOps_readSparse(struct file* file, struct iov_iter *iter, size_t size, loff_t offset)
{
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);

   struct inode* inode = file->f_mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   RemotingIOInfo ioInfo;
   ssize_t readRes;
   loff_t i_size;
   FhgfsOpsErr helperReadRes;
   FhgfsIsizeHints iSizeHints;

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, __func__, "(offset: %lld; size: %lld)",
      (long long)offset, (long long)size);

   readRes = __FhgfsOps_refreshInode(app, inode, NULL, &iSizeHints);
   if(unlikely(readRes) )
      return readRes;

   i_size = i_size_read(inode);
   if(i_size <= offset)
      return 0; // EOF

   // adapt read length to current file length
   size = MIN(size, (unsigned long long)(i_size - offset) );

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   helperReadRes = FhgfsOpsHelper_readOrClearUser(app, iter, size, offset, fileInfo, &ioInfo);

   if(unlikely(helperReadRes != FhgfsOpsErr_SUCCESS) )
      return FhgfsOpsErr_toSysErr(helperReadRes);

   return size;
}


ssize_t FhgfsOps_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
   size_t count = iov_iter_count(to);
   loff_t pos = iocb->ki_pos;

   struct file* file = iocb->ki_filp;
   struct address_space* mapping = file->f_mapping;
   struct inode* inode = mapping->host;

   App* app = FhgfsOps_getApp(inode->i_sb);

   ssize_t retVal;

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, __func__, "(offset: %lld; size: %lld)",
      (long long)pos, (long long)count);

   IGNORE_UNUSED_VARIABLE(pos);
   IGNORE_UNUSED_VARIABLE(count);

   retVal = __FhgfsOps_revalidateMapping(app, inode);
   if(unlikely(retVal) )
   { // error
      return retVal;
   }

   return generic_file_read_iter(iocb, to);
}

static ssize_t FhgfsOps_buffered_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
   return read_common(iocb->ki_filp, to, iov_iter_count(to), &iocb->ki_pos);
}

static ssize_t write_common(struct file *file, struct iov_iter *from, size_t size, loff_t *offsetPointer)
{
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   Config* cfg = App_getConfig(app);

   struct inode* inode = file->f_mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   RemotingIOInfo ioInfo;
   loff_t writeOffset;
   ssize_t writeRes;
   loff_t newMinFileSize; // to update i_size after write

   bool isLocallyLockedAppend =
      FsFileInfo_getAppending(fileInfo) && !Config_getTuneUseGlobalAppendLocks(cfg);
   bool isGloballyLockedAppend =
      FsFileInfo_getAppending(fileInfo) && Config_getTuneUseGlobalAppendLocks(cfg);

   FhgfsOpsHelper_logOpDebug(app, file_dentry(file), inode, __func__, "(offset: %lld; size: %lld)",
      (long long)*offsetPointer, (long long)size);

   inode_lock(inode);
   {
      writeRes = os_generic_write_checks(file, offsetPointer, &size, S_ISBLK(inode->i_mode) );
      if (likely(! writeRes))  // success
         writeRes = file_remove_privs(file);
   }
   inode_unlock(inode);

   if (unlikely(writeRes))
      return writeRes;

   if (app->cfg->tuneCoherentBuffers)
   {
      /* this flush is necessary to ensure that delayed flushing of the page cache does not
       * overwrite the data written here, even though it was written to the file first. */
      writeRes = filemap_write_and_wait(file->f_mapping);
      if (writeRes < 0)
         return writeRes;

      /* ignore the -EBUSY we could receive here, because there is just *no* way we can keep caches
       * coherent without locking everything all the time. if this produces inconsistent data,
       * something must have been racy anyway. */
      invalidate_inode_pages2(file->f_mapping);
      //Increment coherent rw counter
      atomic_inc(&fhgfsInode->coRWInProg);
   }

   if(isLocallyLockedAppend)
   { // appending without global locks => move file offset to end-of-file before writing

      /* note on flush and lock: the flush here must be inside the local lock, but cannot happen at
         the place where we take the global lock (because that might be called from a flush path
         itself), that's why global and local locks are taken at different places. */

      int flushRes;
      FhgfsOpsErr statRes;
      fhgfs_stat fhgfsStat;

      Fhgfsinode_appendLock(fhgfsInode); // L O C K (append)

      flushRes = __FhgfsOps_flush(app, file, false, false, true, false);
      if(unlikely(flushRes < 0) )
      { // flush error
         writeRes = flushRes;
         goto unlockappend_and_exit;
      }

      FhgfsInode_entryInfoReadLock(fhgfsInode); // LOCK EntryInfo

      /* note on stat here: we could pass -1 to _writeCached and remove the stat here, but the
         disadvantage would be that we don't have the correct file offset for i_size then, so we
         leave the stat here for now. */

      statRes = FhgfsOpsRemoting_statDirect(app, FhgfsInode_getEntryInfo(fhgfsInode), &fhgfsStat);

      FhgfsInode_entryInfoReadUnlock(fhgfsInode); // UNLOCK EntryInfo

      if(unlikely(statRes != FhgfsOpsErr_SUCCESS) )
      { // remote stat error
         writeRes = FhgfsOpsErr_toSysErr(statRes);
         goto unlockappend_and_exit;
      }

      *offsetPointer = fhgfsStat.size;
   }

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   writeOffset = isGloballyLockedAppend ? -1 : *offsetPointer;

   writeRes = FhgfsOpsHelper_writeCached(from, size, writeOffset, fhgfsInode, fileInfo, &ioInfo);
   //writeRes = FhgfsOpsRemoting_writefile(from, size, *offsetPointer, &ioInfo);

   if(unlikely(writeRes < 0) )
   { // write error (=> transform negative fhgfs error code to system error code)
      writeRes = FhgfsOpsErr_toSysErr(-writeRes);
      goto unlockappend_and_exit;
   }

   if(!isGloballyLockedAppend)
   { // for (buffered) global append locks, new offset/filesize would be unknown
      newMinFileSize = *offsetPointer + writeRes; // update with old offset to avoid offset==0 check
      *offsetPointer += writeRes;

      FsFileInfo_setLastWriteOffset(fileInfo, *offsetPointer);

      // check current file size and update if necessary (also important for sparse read heuristic)
      spin_lock(&inode->i_lock);
      if(inode->i_size < newMinFileSize)
         i_size_write(inode, newMinFileSize);
      spin_unlock(&inode->i_lock);
   }

   // add to /proc/<pid>/io
   task_io_account_write(writeRes);

unlockappend_and_exit:
   if(isLocallyLockedAppend)
      Fhgfsinode_appendUnlock(fhgfsInode); // U N L O C K (append)

   // Decrement coherent read/write counter
   if (app->cfg->tuneCoherentBuffers)
      atomic_dec(&fhgfsInode->coRWInProg);
   return writeRes;
}

ssize_t FhgfsOps_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
   size_t count = iov_iter_count(from);
   loff_t pos = iocb->ki_pos;

   struct file* file = iocb->ki_filp;
   struct dentry* dentry = file_dentry(file);
   struct inode* inode = file_inode(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   FhgfsIsizeHints iSizeHints;

   ssize_t retVal;
   int writeCheckRes;

   FhgfsOpsHelper_logOpDebug(app, dentry, inode, __func__, "(offset: %lld; size: %lld)",
      (long long)pos, (long long)count);

   if (iocb->ki_pos != pos)
   { /* Similiar to WARN_ON(iocb->ki_pos != pos), as fuse does */
      Logger_logErrFormatted(log, logContext, "Bug: iocb->ki_pos != pos (%lld vs %lld)",
         iocb->ki_pos, pos);

      dump_stack();
   }

   if(iocb->ki_filp->f_flags & O_APPEND)
   { // O_APPEND => flush (for correct size) and refresh file size

      retVal = __FhgfsOps_refreshInode(app, inode, NULL, &iSizeHints);
      if(retVal)
         return retVal;
   }

   writeCheckRes = os_generic_write_checks(file, &pos, &count, S_ISBLK(inode->i_mode) );
   if(unlikely(writeCheckRes) )
      return writeCheckRes;

   if(!count)
      return 0;

   if( (file->f_flags & O_APPEND) && (pos != iocb->ki_pos) )
   {
      /* pos was updated by generic_write_checks (append writes), so we also need to update
       * iocb->ki_pos, otherwise generic_file_aio_write() will call BUG_ON */
      iocb->ki_pos = pos;
   }

   iov_iter_truncate(from, count);

   retVal = generic_file_write_iter(iocb, from);

   if( (retVal >= 0)
      && ( (IS_SYNC(inode) || (iocb->ki_filp->f_flags & O_SYNC) )
         || unlikely(FhgfsInode_getHasWritePageError(fhgfsInode)) ) )
   { // sync I/O => flush and wait
      struct address_space* mapping = inode->i_mapping;

      if(mapping->nrpages)
      {
         int writeRes = filemap_fdatawrite(mapping);
         if(writeRes >= 0)
         {
            int waitRes = filemap_fdatawait(mapping);
            if(waitRes < 0)
               retVal = waitRes;
         }
         else
            retVal = writeRes;
      }

      if (unlikely(FhgfsInode_getHasWritePageError(fhgfsInode) ) && retVal >= 0)
         FhgfsInode_clearWritePageError(fhgfsInode);
   } // end of if(sync)

   return retVal;
}

static ssize_t FhgfsOps_buffered_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
   return write_common(iocb->ki_filp, from, iov_iter_count(from), &iocb->ki_pos);
}


#ifdef KERNEL_HAS_FSYNC_RANGE /* added in vanilla 3.1 */
int FhgfsOps_fsync(struct file* file, loff_t start, loff_t end, int datasync)
{
   struct dentry* dentry = file_dentry(file);
#elif !defined(KERNEL_HAS_FSYNC_DENTRY)
int FhgfsOps_fsync(struct file* file, int datasync)
{
      struct dentry* dentry = file_dentry(file);
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34) */
int FhgfsOps_fsync(struct file* file, struct dentry* dentry, int datasync)
{
#endif // LINUX_VERSION_CODE
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Config* cfg = App_getConfig(app);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOps_fsync";

   int retVal = 0;
   FsObjectInfo* fsObjectInfo;

   struct inode* inode = file_inode(file);

   if(unlikely(Logger_getLogLevel(log) >= 5) )
      FhgfsOpsHelper_logOp(5, app, dentry, inode, logContext);

   fsObjectInfo = __FhgfsOps_getObjectInfo(file);

   // syncing something other than a file?
   if(FsObjectInfo_getObjectType(fsObjectInfo) != FsObjectType_FILE)
      goto clean_up;

   retVal = __FhgfsOps_flush(app, file, false, Config_getTuneRemoteFSync(cfg), true,
      false);

   if(retVal)
      goto clean_up;


clean_up:

   return retVal;
}

/**
 * Flush data from local cache to servers.
 * Note: This method is in fact a local sync (as wait until data have arrived remotely)
 *
 * @param discardCacheOnError whether or not to discard the internal buffer cache in case of an
 * error (typically you will only want to set true here during file close).
 * @param forceRemoteFlush whether or not remote fsync will be executed depends on this value
 * and the corresponding config value (fsync will use true here and flush won't)
 * @param checkSession whether or not a server crash detection must be done on the server
 * @param isClose whether or not the method is called by a close
 * @return negative linux error code on error
 */
int __FhgfsOps_flush(App* app, struct file *file, bool discardCacheOnError,
   bool forceRemoteFlush, bool checkSession, bool isClose)
{
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);
   const char* logContext = __func__;

   struct inode* inode = file->f_mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   RemotingIOInfo ioInfo;
   int filemapWaitRes;
   int inodeWriteRes;
   FhgfsOpsErr flushRes;
   FhgfsOpsErr bumpRes;
   int retVal = 0;
   bool hasWriteHandle = FhgfsInode_hasWriteHandle(fhgfsInode);

   bool doSyncOnClose = Config_getSysSyncOnClose(App_getConfig(app)) && isClose;

   if(unlikely(Logger_getLogLevel(log) >= 5) )
      FhgfsOpsHelper_logOp(Log_SPAM, app, file_dentry(file), inode, logContext);

   if (hasWriteHandle || FhgfsInode_getHasDirtyPages(fhgfsInode) )
   {
      // flush page cache
      inodeWriteRes = write_inode_now(inode, 1);
      filemapWaitRes = filemap_fdatawait(file->f_mapping);
      if(unlikely(inodeWriteRes < 0 || filemapWaitRes < 0) )
      {
         retVal = (inodeWriteRes < 0) ? inodeWriteRes : filemapWaitRes;
         goto clean_up;
      }
   }

   // flush buffer cache

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   flushRes = FhgfsOpsHelper_flushCache(app, fhgfsInode, discardCacheOnError);
   if(unlikely(flushRes != FhgfsOpsErr_SUCCESS) )
   { // error
      retVal = FhgfsOpsErr_toSysErr(flushRes);
      goto clean_up;
   }

   // remote fsync

   if(forceRemoteFlush || (checkSession && Config_getSysSessionCheckOnClose(cfg)) || doSyncOnClose)
   {
      FhgfsOpsErr fsyncRes = FhgfsOpsRemoting_fsyncfile(&ioInfo, forceRemoteFlush, checkSession,
         doSyncOnClose);
      if(unlikely(fsyncRes != FhgfsOpsErr_SUCCESS) )
      {
         retVal = FhgfsOpsErr_toSysErr(fsyncRes);
         goto clean_up;
      }
   }

   if ((cfg->eventLogMask & EventLogMask_FLUSH) && (file->f_flags & (O_ACCMODE | O_TRUNC)))
   {
      struct FileEvent event;

      FileEvent_init(&event, FileEventType_FLUSH, file_dentry(file));

      FhgfsInode_entryInfoReadLock(fhgfsInode);

      bumpRes = FhgfsOpsRemoting_bumpFileVersion(FhgfsOps_getApp(file_dentry(file)->d_sb),
            &fhgfsInode->entryInfo, false, &event);

      FhgfsInode_entryInfoReadUnlock(fhgfsInode);

      FileEvent_uninit(&event);

      if (bumpRes != FhgfsOpsErr_SUCCESS)
         retVal = FhgfsOpsErr_toSysErr(bumpRes);
   }

clean_up:

   return retVal;
}

int FhgfsOps_mmap(struct file* file, struct vm_area_struct* vma)
{
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOps_mmap";
   int locked;
   int retry;
   int max_retry;

   FhgfsIsizeHints iSizeHints;

   int retVal;
   struct inode* inode = file_inode(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   if(unlikely(Logger_getLogLevel(log) >= 5) )
      FhgfsOpsHelper_logOp(5, app, file_dentry(file), inode, logContext);

   locked = 0;
   retry = 0;

   /*
    * If there are reads/writes already in progress, retry for the inode cache
    * lock till MMAP_RETRY_LOCK_EASY iterations. reads/write will anyway
    * flush the cache. So even if mmap can not get the inode cache lock, it can
    * proceed with the operation. If read/writes are not in progress, wait for
    * more iteration before we get the lock. But mmap should not block forever
    * for the cache lock to avoid the deadlock condition.
    * If mmap has to proceed without getting lock, we will print warning message
    * indicating cache might not be coherent.
    */

   max_retry = atomic_read(&fhgfsInode->coRWInProg) > 0 ?
               MMAP_RETRY_LOCK_EASY : MMAP_RETRY_LOCK_HARD;

   if (app->cfg->tuneCoherentBuffers)
   {
      FhgfsOpsErr flushRes;

      do
      {
         locked = FhgfsInode_fileCacheExclusiveTryLock(fhgfsInode);
         if (locked)
            break;

         // Sleep and retry for the lock
         mdelay(10);
         retry++;
      } while (!locked && retry < max_retry);

      if (locked)
      {
         flushRes = __FhgfsOpsHelper_flushCacheUnlocked(app, fhgfsInode, false);
         if (flushRes != FhgfsOpsErr_SUCCESS)
         {
            FhgfsInode_fileCacheExclusiveUnlock(fhgfsInode);
            retVal = FhgfsOpsErr_toSysErr(flushRes);
            goto exit;
         }
      }
      else
         printk_fhgfs_debug(KERN_WARNING,
                            "mmap couldn't flush the cache. Cache might not be coherent\n");
   }

   retVal = generic_file_mmap(file, vma);

   if(!retVal)
      retVal = __FhgfsOps_doRefreshInode(app, inode, NULL, &iSizeHints, true);

   if (app->cfg->tuneCoherentBuffers && locked)
      FhgfsInode_fileCacheExclusiveUnlock(fhgfsInode);

exit:
   LOG_DEBUG_FORMATTED(log, 5, logContext, "result: %d", retVal);

   return retVal;
}


/**
 * @param fsdata can be used to hand any data over to write_end() (but note that the old
 * prepare_write() doesn't have this)
 * @return 0 on success
 */
#ifdef KERNEL_WRITE_BEGIN_HAS_FLAGS
int FhgfsOps_write_begin(struct file* file, struct address_space* mapping,
   loff_t pos, unsigned len, unsigned flags, struct page** pagep, void** fsdata)
#else
int FhgfsOps_write_begin(struct file* file, struct address_space* mapping,
   loff_t pos, unsigned len, struct page** pagep, void** fsdata)
#endif
{
   pgoff_t index = pos >> PAGE_SHIFT;
   loff_t offset = pos & (PAGE_SIZE - 1);
   loff_t page_start = pos & PAGE_MASK;

   loff_t i_size;
   struct page* page;

   int retVal = 0;
   App* app = FhgfsOps_getApp(file_dentry(file)->d_sb);

   // FhgfsOpsHelper_logOpDebug(app, file->f_dentry, __func__, "(offset: %lld; page_start: %lld; len: %u)",
   //   (long long)offset, (long long)page_start, len);
   IGNORE_UNUSED_VARIABLE(app);

#ifdef KERNEL_WRITE_BEGIN_HAS_FLAGS
   page = grab_cache_page_write_begin(mapping, index, flags);
#else
   page = grab_cache_page_write_begin(mapping, index);
#endif

   if(!page)
   {
      retVal = -ENOMEM;
      goto clean_up;
   }

   if(PageUptodate(page) )
      goto clean_up;

   if(len == PAGE_SIZE)
   {
      // two possibilities:
      // a) full page write => no need to read-update the page from the server
      // b) short write (with offset) => will lead to sync write
      goto clean_up;
   }

   i_size = i_size_read(mapping->host);
   if( (page_start >= i_size) ||
       (!offset && ( (pos + len) >= i_size) ) )
   {
      // we don't need to read data beyond the end of the file
      //    => zero it, and set the page up-to-date

      zero_user_segments(page, 0, offset, offset + len, PAGE_SIZE);

      // note: PageChecked means rest of the page (to which is not being written) is up-to-date.
      //       so when our data is written, the whole page is up-to-date.
      SetPageChecked(page);

      goto clean_up;
   }

   // it is read-modify-write, so update the page with the server content
   retVal = FhgfsOpsPages_readpageSync(file, page);

clean_up:
   // clean-up

   *pagep = page;

   return retVal;
}

/**
 * @param copied the amount that was able to be copied ("copied==len" is always true if
 * write_begin() was called with the AOP_FLAG_UNINTERRUPTIBLE flag)
 * @param fsdata whatever write_begin() set here
 * @return < 0 on failure, number of bytes copied into pagecache (<= 'copied') otherwise
 */
int FhgfsOps_write_end(struct file* file, struct address_space* mapping,
   loff_t pos, unsigned len, unsigned copied, struct page* page, void* fsdata)
{
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);

   struct inode* inode = mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   int retVal;

   struct dentry *dentry= file_dentry(file);
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   const char* logContext = __func__;

   // FhgfsOpsHelper_logOpDebug(app, dentry, logContext, "pos: %lld; len: %u; copied: %u",
   //   (long long)pos, len, copied);
   IGNORE_UNUSED_VARIABLE(logContext);

   if(PageChecked(page) )
   {
      // note: see write_begin() for meaning of PageChecked()

      if(copied == len)
         SetPageUptodate(page);

      ClearPageChecked(page);
   }
   else
   if(!PageUptodate(page) && (copied == PAGE_SIZE) )
      SetPageUptodate(page);


   if(!PageUptodate(page) )
   {
      unsigned offset = pos & (PAGE_SIZE - 1);
      char* buf = kmap(page);
      RemotingIOInfo ioInfo;
      ssize_t writeRes;

      FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

      FhgfsInode_incWriteBackCounter(fhgfsInode);

      writeRes = FhgfsOpsRemoting_writefile(&buf[offset], copied, pos, &ioInfo);

      spin_lock(&inode->i_lock);
      FhgfsInode_setLastWriteBackOrIsizeWriteTime(fhgfsInode);
      FhgfsInode_decWriteBackCounter(fhgfsInode);
      spin_unlock(&inode->i_lock);

      if(likely(writeRes > 0) )
      {
         retVal = writeRes;
         pos += writeRes;
      }
      else
         retVal = FhgfsOpsErr_toSysErr(-writeRes);

      kunmap(page);
   }
   else
   {
      retVal = copied;
      pos += copied;

      if (!PageDirty(page) )
      {  // Only add if the page is not dirty yet (don't add the same page twice...)
         FhgfsInode_incNumDirtyPages(fhgfsInode);
      }
      set_page_dirty(page); // could be in the if-condition above, but for safety we set it here

   }

   if(likely(retVal > 0) )
   {
      spin_lock(&inode->i_lock);
      if(pos > inode->i_size)
      {
         FhgfsInode_setPageWriteFlag(fhgfsInode);
         FhgfsInode_setLastWriteBackOrIsizeWriteTime(fhgfsInode);
         FhgfsInode_setNoIsizeDecrease(fhgfsInode);
         i_size_write(inode, pos);
      }
      spin_unlock(&inode->i_lock);
   }

   unlock_page(page);
   put_page(page);

   // clean-up

   // LOG_DEBUG_FORMATTED(log, 5, logContext, "complete. retVal: %d", retVal);
   IGNORE_UNUSED_VARIABLE(log);

   return retVal;
}

static  ssize_t __FhgfsOps_directIO_common(int rw, struct kiocb *iocb, struct iov_iter *iter, loff_t pos)
{
   struct iov_iter bgfsIter = *iter;  // Was a wrapper copy. Now is just a defensive copy. Still needed?
   struct file* file = iocb->ki_filp;
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(file);
   struct dentry* dentry = file_dentry(file);
   struct inode* inode = file_inode(file);
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   RemotingIOInfo ioInfo;

   ssize_t remotingRes;


   const char* logContext = __func__;
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);

   FhgfsOpsHelper_logOpDebug(app, dentry, inode, logContext, "(%s, pos: %lld, nr_seqs: %lld)",
      (rw == WRITE) ? "WRITE" : "READ", (long long)pos);
   IGNORE_UNUSED_VARIABLE(logContext); // non-debug builds

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   if(rw == WRITE)
   { // write
      remotingRes = FhgfsOpsRemoting_writefileVec(&bgfsIter, pos, &ioInfo, false);
   }
   else if(rw == READ)
   { // read
      remotingRes = FhgfsOpsRemoting_readfileVec(&bgfsIter, iov_iter_count(&bgfsIter), pos, &ioInfo, fhgfsInode);

      if( (remotingRes >= 0 && iov_iter_count(&bgfsIter))
            && ( i_size_read(inode) > (pos + remotingRes) ) )
      { // sparse file compatibility mode
         ssize_t readSparseRes = __FhgfsOps_readSparse(file, &bgfsIter, iov_iter_count(&bgfsIter), pos + remotingRes);

         if(unlikely(readSparseRes < 0) )
            remotingRes = readSparseRes;
         else
            remotingRes += readSparseRes;
      }
   }
   else
   {
#ifdef WARN_ONCE
      WARN_ONCE(1, "unexpected: rw value !=READ and !=WRITE. (int value: %d)\n", rw);
#endif
      return -EINVAL;
   }

   //Write back wrapped iter.
   *iter = bgfsIter;

   if(unlikely(remotingRes < 0) )
   { // error occurred
      LOG_DEBUG_FORMATTED(log, 1, logContext, "error: %s",
            FhgfsOpsErr_toErrString(-remotingRes) );
      IGNORE_UNUSED_VARIABLE(log);

      return FhgfsOpsErr_toSysErr(-remotingRes);
   }

   if(rw == WRITE)
      task_io_account_write(remotingRes);
   else
      task_io_account_read(remotingRes);

   return remotingRes;
}

/**
 * Note: This method must be defined because otherwise the kernel rejects open() with O_DIRECT in
 * fs/open.c. However, it is only called indirectoy through the generic file read/write routines
 * (and swapping code), so it should actually never be called for buffered IO.
 *
 * @param rw whether this is a read or a write {READ, WRITE}
 * @param iocb I/O control block with open file handle
 * @param iov I/O buffer vectors (array)
 * @param pos file offset
 * @param nr_segs length of iov array
 */
ssize_t FhgfsOps_directIO(struct kiocb *iocb, struct iov_iter *iter)
{
   int rw = iov_iter_rw(iter);
   loff_t pos = iocb->ki_pos;
   return __FhgfsOps_directIO_common(rw, iocb, iter, pos);
}
