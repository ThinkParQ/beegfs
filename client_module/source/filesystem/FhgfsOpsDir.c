#include <app/App.h>
#include <app/log/Logger.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>
#include <common/toolkit/Time.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <filesystem/FsDirInfo.h>
#include <filesystem/FsFileInfo.h>
#include <toolkit/NoAllocBufferStore.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include "FhgfsOps_versions.h"
#include "FhgfsOpsInode.h"
#include "FhgfsOpsDir.h"
#include "FhgfsOpsSuper.h"
#include "FhgfsOpsHelper.h"


#include <linux/namei.h>

static int __FhgfsOps_revalidateIntent(struct dentry* parentDentry, struct dentry* dentry);

struct dentry_operations fhgfs_dentry_ops =
{
   .d_revalidate   = FhgfsOps_revalidateIntent,
   .d_delete       = FhgfsOps_deleteDentry,
};

/**
 * Called when the dcache has a lookup hit (and wants to know whether the cache data
 * is still valid).
 *
 * @return value is quasi-boolean: 0 if entry invalid, 1 if still valid (no other return values
 * allowed).
 */
#ifndef KERNEL_HAS_ATOMIC_OPEN
int FhgfsOps_revalidateIntent(struct dentry* dentry, struct nameidata* nameidata)
#else
int FhgfsOps_revalidateIntent(struct dentry* dentry, unsigned flags)
#endif // LINUX_VERSION_CODE
{
   App* app;
   Config* cfg;
   Logger* log;
   const char* logContext;

   int isValid = 0; // quasi-boolean (return value)
   struct dentry* parentDentry;
   struct inode* parentInode;
   struct inode* inode;
   struct timespec64 now;
   unsigned long nowTime;
   unsigned cacheValidityMS;

   #ifndef KERNEL_HAS_ATOMIC_OPEN
      unsigned flags = nameidata ? nameidata->flags : 0;

      IGNORE_UNUSED_VARIABLE(flags);
   #endif // LINUX_VERSION_CODE


   #ifdef LOOKUP_RCU
      /* note: 2.6.38 introduced rcu-walk mode, which is inappropriate for us, because we need the
         parentDentry and need to sleep for communication. ECHILD below tells vfs to call this again
         in old ref-walk mode. (see Documentation/filesystems/vfs.txt:d_revalidate) */

      if(flags & LOOKUP_RCU)
         return -ECHILD;
   #endif // LINUX_VERSION_CODE

   inode = dentry->d_inode;

   // access to parentDentry and inode needs to live below the rcu check.

   app = FhgfsOps_getApp(dentry->d_sb);
   cfg = App_getConfig(app);
   log = App_getLogger(app);
   logContext = "FhgfsOps_revalidateIntent";

   parentDentry = dget_parent(dentry);
   parentInode = parentDentry->d_inode;

   if(unlikely(Logger_getLogLevel(log) >= 5) )
      FhgfsOpsHelper_logOp(Log_SPAM, app, dentry, inode, logContext);

   if(!inode || !parentInode || is_bad_inode(inode) )
   {
      if(inode && S_ISDIR(inode->i_mode) )
      {
         if(have_submounts(dentry) )
            goto cleanup_put_parent;

         shrink_dcache_parent(dentry);
      }

      // dentry->d_time is updated with the current time during the first lookup,
      // the cache is only valid if the difference of CURRENT_TIME and revalidate 
      // time is less than the tuneENOENTCacheValidityMS from the config 

      ktime_get_real_ts64(&now);
      cacheValidityMS = Config_getTuneENOENTCacheValidityMS(cfg);
      nowTime = (now.tv_sec * 1000000000UL + now.tv_nsec);
      if (((nowTime - dentry->d_time)/1000000UL) > cacheValidityMS)
      {
         d_drop(dentry);
         goto cleanup_put_parent;
      }
      else
      {
         isValid = 1;
         goto cleanup_put_parent;
      }
   }

   // active dentry => remote-stat and local-compare

   isValid = __FhgfsOps_revalidateIntent(parentDentry, dentry );

cleanup_put_parent:
   // clean-up
   dput(parentDentry);

   LOG_DEBUG_FORMATTED(log, 5, logContext, "'%s': isValid: %s",
      dentry->d_name.name, isValid ? "yes" : "no");

   return isValid;
}

/*
 * sub function of FhgfsOps_revalidateIntent(), supposed to be inlined, as we resolve several
 * pointers two times, in this function and also already in the caller
 *
 * @return value is quasi-boolean: 0 if entry invalid, 1 if still valid (no other return values
 * allowed).
 */
int __FhgfsOps_revalidateIntent(struct dentry* parentDentry, struct dentry* dentry)
{
   const char* logContext = "__FhgfsOps_revalidateIntent";
   App* app = FhgfsOps_getApp(dentry->d_sb);
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);

   const char* entryName = dentry->d_name.name;

   fhgfs_stat fhgfsStat;
   fhgfs_stat* fhgfsStatPtr;

   struct inode* parentInode = parentDentry->d_inode;
   FhgfsInode* parentFhgfsInode = BEEGFS_INODE(parentInode);

   struct inode* inode = dentry->d_inode;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

   bool cacheValid = FhgfsInode_isCacheValid(fhgfsInode, inode->i_mode, cfg);
   int isValid = 0; // quasi-boolean (return value)
   bool needDrop = false;

   FhgfsIsizeHints iSizeHints;


   FhgfsOpsHelper_logOp(Log_SPAM, app, dentry, inode, logContext);


   if (cacheValid)
   {
      isValid = 1;
      return isValid;
   }

   if(IS_ROOT(dentry) )
      fhgfsStatPtr = NULL;
   else
   { // any file or directory except our mount root
      const EntryInfo* parentInfo;
      EntryInfo* entryInfo;

      FhgfsOpsErr remotingRes;
      LookupIntentInfoIn inInfo; // input data for combo-request
      LookupIntentInfoOut outInfo; // result data of combo-request

      FhgfsInode_initIsizeHints(fhgfsInode, &iSizeHints);

      FhgfsInode_entryInfoReadLock(parentFhgfsInode); // LOCK parentInfo
      FhgfsInode_entryInfoWriteLock(fhgfsInode);      // LOCK EntryInfo

      parentInfo = FhgfsInode_getEntryInfo(parentFhgfsInode);
      entryInfo  = &fhgfsInode->entryInfo;

      LookupIntentInfoIn_init(&inInfo, parentInfo, entryName);
      LookupIntentInfoIn_addEntryInfo(&inInfo, entryInfo);

      LookupIntentInfoOut_prepare(&outInfo, NULL, &fhgfsStat);

      remotingRes = FhgfsOpsRemoting_lookupIntent(app, &inInfo, &outInfo);

      // we only need to update entryInfo flags
      if (remotingRes == FhgfsOpsErr_SUCCESS && outInfo.revalidateRes == FhgfsOpsErr_SUCCESS)
         entryInfo->featureFlags = outInfo.revalidateUpdatedFlags;

      FhgfsInode_entryInfoWriteUnlock(fhgfsInode);      // UNLOCK EntryInfo
      FhgfsInode_entryInfoReadUnlock(parentFhgfsInode); // UNLOCK parentInfo

      if (unlikely(remotingRes != FhgfsOpsErr_SUCCESS) )
      {
         needDrop = true;
         goto out;
      }

      if (outInfo.revalidateRes != FhgfsOpsErr_SUCCESS)
      {
         if(unlikely(!(outInfo.responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE) ) )
            Logger_logErrFormatted(log, logContext, "Unexpected revalidate info missing: %s",
               entryInfo->fileName);

         needDrop = true;
         goto out;
      }

      // check the stat result here and set fhgfsStatPtr accordingly
      if(outInfo.statRes == FhgfsOpsErr_SUCCESS)
         fhgfsStatPtr = &fhgfsStat; // successful, so we can use existing stat values
      else
      if(outInfo.statRes == FhgfsOpsErr_NOTOWNER)
         fhgfsStatPtr = NULL; // stat values not available
      else
      {
         if(unlikely(!(outInfo.responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT) ) )
            Logger_logErrFormatted(log, logContext, "Unexpected stat info missing: %s",
               entryInfo->fileName);

         // now its getting difficult as there is an unexpected error
         needDrop = true;
         goto out;
      }
   }

   if (!__FhgfsOps_refreshInode(app, inode, fhgfsStatPtr, &iSizeHints) )
      isValid = 1;
   else
      isValid = 0;

out:
   if (needDrop)
      d_drop(dentry);

   return isValid;
}

/**
 * This is called from dput() when d_count is going to 0 and dput() wants to know from us whether
 * not it should delete the dentry.
 *
 * @return !=0 to delete dentry, 0 to keep it
 */
#ifndef KERNEL_HAS_D_DELETE_CONST_ARG
   int FhgfsOps_deleteDentry(struct dentry* dentry)
#else
   int FhgfsOps_deleteDentry(const struct dentry* dentry)
#endif // LINUX_VERSION_CODE
{
   int shouldBeDeleted = 0; // quasi-boolean (return value)
   struct inode* inode = dentry->d_inode;

   // For both positive and negative dentry cases,
   // dentry cache (dcache) is mainatined

   if(inode)
   {
      if(is_bad_inode(inode) )
         shouldBeDeleted = 1; // inode marked as bad => no need to keep dentry
   }

   return shouldBeDeleted;
}

/*
 * Constructs the path from the root dentry (of the mount-point) to an arbitrary hashed dentry.
 *
 * Note: Acquires a buf from the pathBufStore that must be released by the caller.
 * Note: This is safe for paths larger than bufSize, but returns -ENAMETOOLONG in that case
 * NOTE: Do NOT call two times in the same thread, as it might deadlock if multiple threads
 *       try to aquire a buffer. The thread that takes the buffer two times might never finish,
 *       if not sufficient buffers are available. If multiple threads take multiple buffers in
 *       parallel an infinity deadlock of the filesystem will happen.
 *
 * @outBuf the buf that must be returned to the pathBufStore of the app
 * @return some offset within the outStoreBuf or the linux ERR_PTR(errorCode) (already negative) and
 * *outStoreBuf will be NULL then
 */
char* __FhgfsOps_pathResolveToStoreBuf(NoAllocBufferStore* bufStore, struct dentry* dentry,
   char** outStoreBuf)
{
   char * path;
   const ssize_t storeBufLen = NoAllocBufferStore_getBufSize(bufStore);
   *outStoreBuf = NoAllocBufferStore_waitForBuf(bufStore);

   path = dentry_path_raw(dentry, *outStoreBuf, storeBufLen);
   if (unlikely (IS_ERR(path) ) )
   {
      NoAllocBufferStore_addBuf(bufStore, *outStoreBuf);
      *outStoreBuf = NULL;
   }

   return path;
}
