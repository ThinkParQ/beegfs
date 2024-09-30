#ifndef FHGFSOPSINODE_H_
#define FHGFSOPSINODE_H_

#include <app/App.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/Time.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <filesystem/FsDirInfo.h>
#include <filesystem/FsFileInfo.h>
#include <toolkit/NoAllocBufferStore.h>
#include <common/Common.h>
#include "FhgfsInode.h"
#include "FhgfsOps_versions.h"

#include <linux/fs.h>
#include <linux/vfs.h>

// forward declaration
struct App;

struct FhgfsInodeComparisonInfo;
typedef struct FhgfsInodeComparisonInfo FhgfsInodeComparisonInfo;

#if defined(KERNEL_HAS_CURRENT_TIME_SPEC64)
typedef struct timespec64 inode_timespec;
#else
typedef struct timespec inode_timespec;
#endif

#if ! defined(KERNEL_HAS_INODE_GET_SET_CTIME)
/* These functions have been adapted from kernel code to work for older kernels.
The function signatures were introduced in version ~6.5 */

static inline time64_t inode_get_ctime_sec(const struct inode *inode)
{
   return inode->i_ctime.tv_sec;
}

static inline long inode_get_ctime_nsec(const struct inode *inode)
{
   return inode->i_ctime.tv_nsec;
}

static inline inode_timespec inode_get_ctime(const struct inode *inode)
{
   return inode->i_ctime;
}

static inline inode_timespec inode_set_ctime_to_ts(struct inode *inode,
                        inode_timespec ts)
{
   inode->i_ctime = ts;
   return ts;
}

static inline inode_timespec inode_set_ctime(struct inode *inode,
                  time64_t sec, long nsec)
{
   inode_timespec ts = { .tv_sec  = sec,
             .tv_nsec = nsec };

   return inode_set_ctime_to_ts(inode, ts);
}
#endif

#if ! defined(KERNEL_HAS_INODE_GET_SET_CTIME_MTIME_ATIME)
/* These functions have been adapted from kernel code to work for older kernels.
The function signatures were introduced in version ~6.6 */

static inline time64_t inode_get_atime_sec(const struct inode *inode)
{
   return inode->i_atime.tv_sec;
}

static inline long inode_get_atime_nsec(const struct inode *inode)
{
   return inode->i_atime.tv_nsec;
}

static inline inode_timespec inode_get_atime(const struct inode *inode)
{
   return inode->i_atime;
}

static inline inode_timespec inode_set_atime_to_ts(struct inode *inode,
                        inode_timespec ts)
{
   inode->i_atime = ts;
   return ts;
}

static inline inode_timespec inode_set_atime(struct inode *inode,
                  time64_t sec, long nsec)
{
   inode_timespec ts = { .tv_sec  = sec,
             .tv_nsec = nsec };
   return inode_set_atime_to_ts(inode, ts);
}

static inline time64_t inode_get_mtime_sec(const struct inode *inode)
{
   return inode->i_mtime.tv_sec;
}

static inline long inode_get_mtime_nsec(const struct inode *inode)
{
   return inode->i_mtime.tv_nsec;
}

static inline inode_timespec inode_get_mtime(const struct inode *inode)
{
   return inode->i_mtime;
}

static inline inode_timespec inode_set_mtime_to_ts(struct inode *inode,
                        inode_timespec ts)
{
   inode->i_mtime = ts;
   return ts;
}

static inline inode_timespec inode_set_mtime(struct inode *inode,
                  time64_t sec, long nsec)
{
   inode_timespec ts = { .tv_sec  = sec,
             .tv_nsec = nsec };
   return inode_set_mtime_to_ts(inode, ts);
}
#endif

static inline void inode_set_mc_time(struct inode *inode,
                  inode_timespec ts)
{
     inode_set_mtime_to_ts(inode, ts);
     inode_set_ctime_to_ts(inode, ts);
}

#ifndef KERNEL_HAS_ATOMIC_OPEN
   extern struct dentry* FhgfsOps_lookupIntent(struct inode* parentDir, struct dentry* dentry,
      struct nameidata* nameidata);
#else
   extern struct dentry* FhgfsOps_lookupIntent(struct inode* parentDir, struct dentry* dentry,
      unsigned flags);
#endif // KERNEL_HAS_ATOMIC_OPEN

#ifdef KERNEL_HAS_STATX
#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
extern int FhgfsOps_getattr(struct mnt_idmap* idmap, const struct path* path,
      struct kstat* kstat, u32 request_mask, unsigned int query_flags);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
extern int FhgfsOps_getattr(struct user_namespace* ns, const struct path* path,
      struct kstat* kstat, u32 request_mask, unsigned int query_flags);
#else
extern int FhgfsOps_getattr(const struct path* path, struct kstat* kstat, u32 request_mask,
      unsigned int query_flags);
#endif
#else
extern int FhgfsOps_getattr(struct vfsmount* mnt, struct dentry* dentry, struct kstat* kstat);
#endif

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
extern int FhgfsOps_setattr(struct mnt_idmap* idmap, struct dentry* dentry, struct iattr* iattr);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
extern int FhgfsOps_setattr(struct user_namespace* ns, struct dentry* dentry, struct iattr* iattr);
#else
extern int FhgfsOps_setattr(struct dentry* dentry, struct iattr* iattr);
#endif

extern ssize_t FhgfsOps_listxattr(struct dentry* dentry, char* value, size_t size);
#ifdef KERNEL_HAS_DENTRY_XATTR_HANDLER
extern ssize_t FhgfsOps_getxattr(struct dentry* dentry, const char* name, void* value, size_t size);
extern int FhgfsOps_setxattr(struct dentry* dentry, const char* name, const void* value,
      size_t size, int flags);
#else
extern ssize_t FhgfsOps_getxattr(struct inode* inode, const char* name, void* value, size_t size);
extern int FhgfsOps_setxattr(struct inode* inode, const char* name, const void* value,
      size_t size, int flags);
#endif // KERNEL_HAS_DENTRY_XATTR_HANDLER
extern int FhgfsOps_removexattr(struct dentry* dentry, const char* name);
extern int FhgfsOps_removexattrInode(struct inode* inode, const char* name);

#if defined(KERNEL_HAS_GET_INODE_ACL)
extern struct posix_acl* FhgfsOps_get_inode_acl(struct inode* inode, int type, bool rcu);
#endif
#ifdef KERNEL_HAS_GET_ACL
#if defined(KERNEL_HAS_POSIX_GET_ACL_IDMAP)
extern struct posix_acl * FhgfsOps_get_acl(struct mnt_idmap *idmap, struct dentry *dentry, int type);
#elif defined(KERNEL_HAS_POSIX_GET_ACL_NS)
extern struct posix_acl * FhgfsOps_get_acl(struct user_namespace *userns, struct dentry *dentry, int type);
#elif defined(KERNEL_POSIX_GET_ACL_HAS_RCU)
extern struct posix_acl* FhgfsOps_get_acl(struct inode* inode, int type, bool rcu);
#else
extern struct posix_acl* FhgfsOps_get_acl(struct inode* inode, int type);
#endif

int FhgfsOps_aclChmod(struct iattr* iattr, struct dentry* dentry);
#endif

#if defined(KERNEL_HAS_SET_ACL)
#if defined(KERNEL_HAS_SET_ACL_DENTRY)

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
extern int FhgfsOps_set_acl(struct mnt_idmap* mnt_userns, struct dentry* dentry,
   struct posix_acl* acl, int type);
#else
extern int FhgfsOps_set_acl(struct user_namespace* mnt_userns, struct dentry* dentry,
   struct posix_acl* acl, int type);
#endif

#else

#if defined(KERNEL_HAS_SET_ACL_NS_INODE)
extern int FhgfsOps_set_acl(struct user_namespace* mnt_userns, struct inode* inode,
   struct posix_acl* acl, int type);
#else
extern int FhgfsOps_set_acl(struct inode* inode, struct posix_acl* acl, int type);
#endif

#endif //KERNEL_HAS_SET_ACL_DENTRY
#endif // KERNEL_HAS_SET_ACL

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
extern int FhgfsOps_mkdir(struct mnt_idmap* idmap, struct inode* dir,
   struct dentry* dentry, umode_t mode);
extern int FhgfsOps_mknod(struct mnt_idmap* idmap, struct inode* dir,
   struct dentry* dentry, umode_t mode, dev_t dev);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
extern int FhgfsOps_mkdir(struct user_namespace* mnt_userns, struct inode* dir,
   struct dentry* dentry, umode_t mode);
extern int FhgfsOps_mknod(struct user_namespace* mnt_userns, struct inode* dir,
   struct dentry* dentry, umode_t mode, dev_t dev);
#elif defined(KERNEL_HAS_UMODE_T)
extern int FhgfsOps_mkdir(struct inode* dir, struct dentry* dentry, umode_t mode);
extern int FhgfsOps_mknod(struct inode* dir, struct dentry* dentry, umode_t mode, dev_t dev);
#else
extern int FhgfsOps_mkdir(struct inode* dir, struct dentry* dentry, int mode);
extern int FhgfsOps_mknod(struct inode* dir, struct dentry* dentry, int mode, dev_t dev);
#endif

#if defined KERNEL_HAS_ATOMIC_OPEN
   int FhgfsOps_atomicOpen(struct inode* dir, struct dentry* dentry, struct file* file,
      unsigned openFlags, umode_t createMode
      #ifndef FMODE_CREATED
      ,  int* outOpenedFlags
      #endif
      );
   #if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
   extern int FhgfsOps_createIntent(struct mnt_idmap* idmap, struct inode* dir,
      struct dentry* dentry, umode_t mode, bool isExclusiveCreate);
   #elif defined(KERNEL_HAS_USER_NS_MOUNTS)
   extern int FhgfsOps_createIntent(struct user_namespace* mnt_userns, struct inode* dir,
      struct dentry* dentry, umode_t mode, bool isExclusiveCreate);
   #else
   extern int FhgfsOps_createIntent(struct inode* dir, struct dentry* dentry, umode_t mode,
      bool isExclusiveCreate);
   #endif
#elif defined KERNEL_HAS_UMODE_T
   extern int FhgfsOps_createIntent(struct inode* dir, struct dentry* dentry, umode_t mode,
      struct nameidata* nameidata);
#else
   extern int FhgfsOps_createIntent(struct inode* dir, struct dentry* dentry, int mode,
      struct nameidata* nameidata);
#endif // KERNEL_HAS_ATOMIC_OPEN

extern int FhgfsOps_rmdir(struct inode* dir, struct dentry* dentry);
extern int FhgfsOps_unlink(struct inode* dir, struct dentry* dentry);

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
extern int FhgfsOps_symlink(struct mnt_idmap* idmap, struct inode* dir,
   struct dentry* dentry, const char* to);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
extern int FhgfsOps_symlink(struct user_namespace* mnt_userns, struct inode* dir,
   struct dentry* dentry, const char* to);
#else
extern int FhgfsOps_symlink(struct inode* dir, struct dentry* dentry, const char* to);
#endif

extern int FhgfsOps_link(struct dentry* dentryFrom, struct inode* inode, struct dentry* dentryTo);
extern int FhgfsOps_hardlinkAsSymlink(struct dentry* oldDentry, struct inode* dir,
   struct dentry* newDentry);


#if defined KERNEL_HAS_GET_LINK
extern const char* FhgfsOps_get_link(struct dentry* dentry, struct inode* inode,
   struct delayed_call* done);
#elif defined(KERNEL_HAS_FOLLOW_LINK_COOKIE)
extern const char* FhgfsOps_follow_link(struct dentry* dentry, void** cookie);
extern void FhgfsOps_put_link(struct inode* inode, void* cookie);
#else
extern void* FhgfsOps_follow_link(struct dentry* dentry, struct nameidata* nd);
extern void FhgfsOps_put_link(struct dentry* dentry, struct nameidata* nd, void* p);
#endif

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
extern int FhgfsOps_rename(struct mnt_idmap* idmap, struct inode* inodeDirFrom,
   struct dentry* dentryFrom, struct inode* inodeDirTo, struct dentry* dentryTo, unsigned flags);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
extern int FhgfsOps_rename(struct user_namespace* mnt_userns, struct inode* inodeDirFrom,
   struct dentry* dentryFrom, struct inode* inodeDirTo, struct dentry* dentryTo, unsigned flags);
#elif defined(KERNEL_HAS_RENAME_FLAGS)
extern int FhgfsOps_rename(struct inode* inodeDirFrom, struct dentry* dentryFrom,
   struct inode* inodeDirTo, struct dentry* dentryTo, unsigned flags);
#else
extern int FhgfsOps_rename(struct inode* inodeDirFrom, struct dentry* dentryFrom,
   struct inode* inodeDirTo, struct dentry* dentryTo);
#endif

extern int FhgfsOps_vmtruncate(struct inode* inode, loff_t offset);


extern bool FhgfsOps_initInodeCache(void);
extern void FhgfsOps_destroyInodeCache(void);
extern struct inode* FhgfsOps_alloc_inode(struct super_block* sb);
extern void FhgfsOps_destroy_inode(struct inode* inode);

extern struct inode* __FhgfsOps_newInodeWithParentID(struct super_block* sb, struct kstat* kstat,
   dev_t dev, EntryInfo* entryInfo, NumNodeID parentNodeID, FhgfsIsizeHints* iSizeHints);

extern int __FhgfsOps_instantiateInode(struct dentry* dentry, EntryInfo* entryInfo,
   fhgfs_stat* fhgfsStat, FhgfsIsizeHints* iSizeHints);
int __FhgfsOps_compareInodeID(struct inode* cachedInode, void* newInodeInfo);
int __FhgfsOps_initNewInodeDummy(struct inode* newInode, void* newInodeInfo);

static inline int __FhgfsOps_refreshInode(App* app, struct inode* inode, fhgfs_stat* fhgfsStat,
   FhgfsIsizeHints* iSizeHints);
extern int __FhgfsOps_doRefreshInode(App* app, struct inode* inode, fhgfs_stat* fhgfsStat,
   FhgfsIsizeHints* iSizeHints, bool noFlush);
extern int __FhgfsOps_revalidateMapping(App* app, struct inode* inode);
extern int __FhgfsOps_flushInodeFileCache(App* app, struct inode* inode);


// inliners
static inline void __FhgfsOps_applyStatDataToInode(struct kstat* kstat, FhgfsIsizeHints* iSizeHints,
   struct inode* outInode);
static inline void __FhgfsOps_applyStatDataToInodeUnlocked(struct kstat* kstat,
   FhgfsIsizeHints* iSizeHints, struct inode* outInode);
static inline void __FhgfsOps_applyStatAttribsToInode(struct kstat* kstat, struct inode* outInode);
static inline void __FhgfsOps_applyStatSizeToInode(struct kstat* kstat,
   FhgfsIsizeHints* iSizeHints, struct inode* inOutInode);
static inline struct inode* __FhgfsOps_newInode(struct super_block* sb, struct kstat* kstat,
   dev_t dev, EntryInfo* entryInfo,  FhgfsIsizeHints* iSizeHints);
static inline bool __FhgfsOps_isPagedMode(struct super_block* sb);

/**
 * This structure is passed to _compareInodeID().
 */
struct FhgfsInodeComparisonInfo
{
   ino_t inodeHash; // (=> inode::i_ino)
   const char* entryID;
};


/**
 * Note: acquires i_lock spinlock to protect i_size and i_blocks
 */
void __FhgfsOps_applyStatDataToInode(struct kstat* kstat, FhgfsIsizeHints* iSizeHints,
   struct inode* outInode)
{
   __FhgfsOps_applyStatAttribsToInode(kstat, outInode);

   spin_lock(&outInode->i_lock); // I _ L O C K

   __FhgfsOps_applyStatSizeToInode(kstat, iSizeHints, outInode);

   spin_unlock(&outInode->i_lock); // I _ U N L O C K
}

/**
 * Note: Caller must hold i_lock.
 */
void __FhgfsOps_applyStatDataToInodeUnlocked(struct kstat* kstat, FhgfsIsizeHints* iSizeHints,
   struct inode* outInode)
{
   __FhgfsOps_applyStatAttribsToInode(kstat, outInode);

   __FhgfsOps_applyStatSizeToInode(kstat, iSizeHints, outInode);
}

/**
 * Note: Don't call this directly - use the _applyStatDataToInode... wrappers.
 */
void __FhgfsOps_applyStatAttribsToInode(struct kstat* kstat, struct inode* outInode)
{
   App* app = FhgfsOps_getApp(outInode->i_sb);
   Config* cfg = App_getConfig(app);

   // remote attribs (received from nodes)
   outInode->i_mode = kstat->mode;
   outInode->i_uid = kstat->uid;
   outInode->i_gid = kstat->gid;
   inode_set_atime_to_ts(outInode, kstat->atime);
   inode_set_mtime_to_ts(outInode, kstat->mtime);
   inode_set_ctime_to_ts(outInode, kstat->ctime);

   set_nlink(outInode, kstat->nlink);

   outInode->i_blkbits = Config_getTuneInodeBlockBits(cfg);
}

/**
 * Note: Don't call this directly - use the _applyStatDataToInode... wrappers.
 * Note: Caller must hold i_lock.
 *
 * @param iSizeHints might be NULL
 */
void __FhgfsOps_applyStatSizeToInode(struct kstat* kstat, FhgfsIsizeHints* iSizeHints,
   struct inode* inOutInode)
{
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inOutInode);

   if (S_ISREG(inOutInode->i_mode) && FhgfsInode_getHasPageWriteFlag(fhgfsInode) )
   {
      loff_t oldSize = i_size_read(inOutInode);
      uint64_t lastWriteBackOrIsizeWriteTime;

      /* We don't allow to decrease the file size in paged mode, as
       * this may/will cause data corruption (zeros/holes instead of real data).
       * The detection when to apply the server i_size is rather complex. */
      if (oldSize > kstat->size)
      {
         if (FhgfsInode_getNoIsizeDecrease(fhgfsInode) )
            return;

         // there are current write-back threads running
         if (FhgfsInode_getWriteBackCounter(fhgfsInode) )
            return;

         // must be read after reading the write-back counter
         lastWriteBackOrIsizeWriteTime = FhgfsInode_getLastWriteBackOrIsizeWriteTime(fhgfsInode);

         if (iSizeHints)
         {
            if (iSizeHints->ignoreIsize)
               return; // iSizeHints tells us to ignore the value

            if (time_after_eq((unsigned long) lastWriteBackOrIsizeWriteTime,
                           (unsigned long) iSizeHints->timeBeforeRemoteStat) )
               return; /* i_size was updated or a writeback thread finished after we send the
                        * remote stat call, so concurrent stat/isize updates and we need to ignore
                          the remote value. */
         }

          #ifdef BEEGFS_DEBUG
          {
             static bool didLog = false;

             struct super_block* sb = inOutInode->i_sb;
             App* app = FhgfsOps_getApp(sb);
             Logger* log = App_getLogger(app);

             if (!didLog)
             {
                // unlock, as the logger gets a mutex lock -> possible lock order issue
                spin_unlock(&inOutInode->i_lock);

                LOG_DEBUG_FORMATTED(log, Log_WARNING, __func__,
                   "Warn-once: (possibly valid) isize-shrink oldSize: %lld newSize: %lld "
                   "lastWBTime: %llu, timeBeforeRemote; %llu\n",
                   oldSize, kstat->size, lastWriteBackOrIsizeWriteTime, iSizeHints->timeBeforeRemoteStat);
                dump_stack();

                didLog = true;

                spin_lock(&inOutInode->i_lock);
             }
          }
          #endif
      }
   }

   i_size_write(inOutInode, kstat->size);
   inOutInode->i_blocks = kstat->blocks;
}

/**
 * See __FhgfsOps_newInodeWithParentID for details. This is just a wrapper function.
 */
struct inode* __FhgfsOps_newInode(struct super_block* sb, struct kstat* kstat, dev_t dev,
   EntryInfo* entryInfo,  FhgfsIsizeHints* iSizeHints)
{
      return __FhgfsOps_newInodeWithParentID(sb, kstat, dev, entryInfo, (NumNodeID){0}, iSizeHints);
}

bool __FhgfsOps_isPagedMode(struct super_block* sb)
{
   App* app = FhgfsOps_getApp(sb);
   Config* cfg = App_getConfig(app);

   if (Config_getTuneFileCacheTypeNum(cfg) == FILECACHETYPE_Paged)
      return true;

   if (Config_getTuneFileCacheTypeNum(cfg) == FILECACHETYPE_Native)
      return true;

   return false;
}

/**
 * See __FhgfsOps_doRefreshInode for details.
 */
int __FhgfsOps_refreshInode(App* app, struct inode* inode, fhgfs_stat* fhgfsStat,
   FhgfsIsizeHints* iSizeHints)
{
   // do not disable inode flushing from this (default) refreshInode function
   return __FhgfsOps_doRefreshInode(app, inode, fhgfsStat, iSizeHints, false);
}



#endif /* FHGFSOPSINODE_H_ */
