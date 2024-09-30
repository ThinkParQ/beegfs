#include <app/log/Logger.h>
#include <app/App.h>
#include <common/Common.h>
#include <filesystem/FhgfsOpsHelper.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <os/OsCompat.h>
#include <toolkit/NoAllocBufferStore.h>
#include <toolkit/StatFsCache.h>
#include "FhgfsOps_versions.h"
#include "FhgfsOpsFile.h"
#include "FhgfsOpsDir.h"
#include "FhgfsOpsInode.h"
#include "FhgfsOpsSuper.h"


/**
  * A basic permission() method. Only required to tell the VFS we do not support RCU path walking.
  *
  * @return 0 on success, negative linux error code otherwise
  */
#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
   int FhgfsOps_permission(struct mnt_idmap* idmap, struct inode *inode, int mask)
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
   int FhgfsOps_permission(struct user_namespace* mnt_userns, struct inode *inode, int mask)
#elif defined(KERNEL_HAS_PERMISSION_2)
   int FhgfsOps_permission(struct inode *inode, int mask)
#elif defined(KERNEL_HAS_PERMISSION_FLAGS)
   int FhgfsOps_permission(struct inode *inode, int mask, unsigned int flags)
#else
   /* <= 2.6.26 */
   int FhgfsOps_permission(struct inode *inode, int mask, struct nameidata *nd)
#endif
{
    /* note: 2.6.38 introduced rcu-walk mode, which is inappropriate for us, because we need the
       parent. the code below tells vfs to call this again in old ref-walk mode.
       (see Documentation/filesystems/vfs.txt:d_revalidate) */
   #ifdef MAY_NOT_BLOCK
      if(mask & MAY_NOT_BLOCK)
         return -ECHILD;
   #elif defined(IPERM_FLAG_RCU)
      if(flags & IPERM_FLAG_RCU)
         return -ECHILD;
   #endif // LINUX_VERSION_CODE

   return os_generic_permission(inode, mask);
}


int FhgfsOps_statfs(struct dentry* dentry, struct kstatfs* kstatfs)
{
   struct super_block* sb = dentry->d_sb;

   const char* logContext = __func__;

   App* app = FhgfsOps_getApp(sb);
   Logger* log = App_getLogger(app);
   StatFsCache* statfsCache = App_getStatFsCache(app);

   int retVal = -EREMOTEIO;
   FhgfsOpsErr statRes;

   int64_t sizeTotal = 0;
   int64_t sizeFree = 0;

   FhgfsOpsHelper_logOp(Log_SPAM, app, NULL, NULL, logContext);


   memset(kstatfs, 0, sizeof(struct kstatfs) );

   kstatfs->f_type      = BEEGFS_MAGIC;

   kstatfs->f_namelen   = NAME_MAX;
   kstatfs->f_files     = 0; // total used file nodes (not supported currently)
   kstatfs->f_ffree     = 0; // free file nodes (not supported currently)

   kstatfs->f_bsize     = BEEGFS_STATFS_BLOCKSIZE;
   kstatfs->f_frsize    = BEEGFS_STATFS_BLOCKSIZE; /* should be same as f_bsize, as some versions of
      glibc do not correctly handle frsize!=bsize (both are used inconsistently as a unit for
      f_blocks & co) */

   statRes = StatFsCache_getFreeSpace(statfsCache, &sizeTotal, &sizeFree);

   LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "remoting complete. result: %d", (int)statRes);
   IGNORE_UNUSED_VARIABLE(log);

   retVal = FhgfsOpsErr_toSysErr(statRes);

   if(statRes == FhgfsOpsErr_SUCCESS)
   { // success => assign received values

      // note: f_blocks, f_bfree, and f_bavail are reported in units of f_bsize.

      unsigned char blockBits = BEEGFS_STATFS_BLOCKSIZE_SHIFT; // just a shorter name
      unsigned long blockSizeDec = (1 << blockBits) - 1; // for rounding

      kstatfs->f_blocks = (sizeTotal + blockSizeDec) >> blockBits;
      kstatfs->f_bfree  = (sizeFree  + blockSizeDec) >> blockBits; // free for superuser
      kstatfs->f_bavail = (sizeFree  + blockSizeDec) >> blockBits; // available for non-superuser
   }

   return retVal;
}



#ifdef KERNEL_HAS_GET_SB_NODEV

int FhgfsOps_getSB(struct file_system_type *fs_type,
   int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
   return get_sb_nodev(fs_type, flags, data, FhgfsOps_fillSuper, mnt);
}

#else

/* kernel 2.6.39 switched from get_sb() to mount(), which provides similar functionality from our
   point of view. */

struct dentry* FhgfsOps_mount(struct file_system_type *fs_type,
   int flags, const char *dev_name, void *data)
{
   return mount_nodev(fs_type, flags, data, FhgfsOps_fillSuper);
}

#endif // LINUX_VERSION_CODE


/**
 * Note: Called by close(2) system call before the actual FhgfsOps_close().
 */
int FhgfsOps_flush(struct file *file, fl_owner_t id)
{
   struct dentry* dentry = file_dentry(file);
   struct inode* inode = file_inode(file);

   App* app = FhgfsOps_getApp(dentry->d_sb);
   const char* logContext = "FhgfsOps_flush";

   FhgfsOpsHelper_logOp(Log_SPAM, app, dentry, inode, logContext);


   /* note: if a buffer cannot be flushed here, we still have the inode in the InodeRefStore, so
      that the flusher can write it asynchronously */


   return __FhgfsOps_flush(app, file, false, false, true, true);
}

/**
 * Note: Called for new cache objects (see FhgfsOps_initInodeCache() )
 */
#ifdef SLAB_CTOR_CONSTRUCTOR

void FhgfsOps_initInodeOnce(void* inode, struct kmem_cache* cache, unsigned long flags)
{
   // note: no, this is not a typo. since kernel version 2.6.22, we're no longer checking
   //    the slab_... flags (even though the argument still exists in the method signature).

   if( (flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR) ) == SLAB_CTOR_CONSTRUCTOR)
   { // fresh construction => initialize object
      FhgfsInode* fhgfsInode = (FhgfsInode*)inode;

      FhgfsInode_initOnce(fhgfsInode);

      inode_init_once(&fhgfsInode->vfs_inode);
   }
}

#else

   #if defined(KERNEL_HAS_KMEMCACHE_CACHE_FLAGS_CTOR)
   void FhgfsOps_initInodeOnce(void* inode, struct kmem_cache* cache, unsigned long flags)
   #elif defined(KERNEL_HAS_KMEMCACHE_CACHE_CTOR)
   void FhgfsOps_initInodeOnce(struct kmem_cache* cache, void* inode)
   #else
   void FhgfsOps_initInodeOnce(void* inode)
   #endif // LINUX_VERSION_CODE

   {
      FhgfsInode* fhgfsInode = (FhgfsInode*)inode;

      FhgfsInode_initOnce(fhgfsInode);

      inode_init_once(&fhgfsInode->vfs_inode);
   }

#endif // LINUX_VERSION_CODE


#ifndef KERNEL_HAS_GENERIC_FILE_LLSEEK_UNLOCKED
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)

/**
 * Note: This is just an exact copy of the kernel method (from kernel 2.6.30), which was introduced
 * in 2.6.27.
 *
 * generic_file_llseek_unlocked - lockless generic llseek implementation
 * @file:   file structure to seek on
 * @offset: file offset to seek to
 * @origin: type of seek
 *
 * Updates the file offset to the value specified by @offset and @origin.
 * Locking must be provided by the caller.
 */
loff_t generic_file_llseek_unlocked(struct file *file, loff_t offset, int origin)
{
   struct inode *inode = file->f_mapping->host;

   switch (origin)
   {
      case SEEK_END:
         offset += inode->i_size;
         break;
      case SEEK_CUR:
         /*
          * Here we special-case the lseek(fd, 0, SEEK_CUR)
          * position-querying operation.  Avoid rewriting the "same"
          * f_pos value back to the file because a concurrent read(),
          * write() or lseek() might have altered it
          */
         if (offset == 0)
            return file->f_pos;
         offset += file->f_pos;
         break;
   }

   if (offset < 0 || offset > (loff_t) inode->i_sb->s_maxbytes)
      return -EINVAL;

   /* Special lock needed here? */
   if (offset != file->f_pos)
   {
      file->f_pos = offset;
      file->f_version = 0;
   }

   return offset;
}

#else

/**
 * Note: The lock (to which the _unlocked suffix refers) was dropped in linux 3.2, so we can now
 * just call the kernel method without the _unlocked suffix.
 */
loff_t generic_file_llseek_unlocked(struct file *file, loff_t offset, int origin)
{
   return generic_file_llseek(file, offset, origin);
}

#endif // LINUX_VERSION_CODE
#endif

#ifndef KERNEL_HAS_DENTRY_PATH_RAW
/**
 * Get the relative path of a dentry to the mount point
 *
 * @param dentry The dentry we want the path for
 * @param buf    A preallocated buffer
 * @param buflen The size of buf
 *
 * This is a compatibility function for kernels before 2.6.38.
 * Alternatively, using d_path() is not easy, as we do not have
 * struct vfsmnt. Unfortunately, the kernel interface to get vfsmnt got frequently modified,
 * which would require us to have even more compatibility code.
 * NOTE: We use the vfs global dcache_lock here, which is rather slow and which will lock up all
 *       file systems if something goes wrong
 * How it works:
 * We build up the path backwards from the end of char * buf, put a "/" before it it and return
 * the pointer to "/"
 */
char *dentry_path_raw(struct dentry *dentry, char *buf, int buflen)
{
   char* outStoreBuf = buf;
   const ssize_t storeBufLen = buflen;
   ssize_t bufLenLeft = storeBufLen; // remaining length
   char* currentBufStart = (outStoreBuf) + bufLenLeft;
   int nameLen;


   *--currentBufStart = '\0';
   bufLenLeft--;

   spin_lock(&dcache_lock);

   while(!IS_ROOT(dentry) )
   {
      nameLen = dentry->d_name.len;
      bufLenLeft -= nameLen + 1;

      if (bufLenLeft < 0)
         goto err_too_long_unlock;

      currentBufStart -= nameLen;

      memcpy(currentBufStart, dentry->d_name.name, nameLen);

      *--currentBufStart = '/';

      dentry = dentry->d_parent;
   }

   spin_unlock(&dcache_lock);

   // return "/" instead of an empty string for the root directory
   if(bufLenLeft == (storeBufLen-1) ) // -1 for the terminating zero
   { // dentry is (mount) root directory
      *--currentBufStart = '/';
   }

   return currentBufStart;

err_too_long_unlock:
   spin_unlock(&dcache_lock);

   return ERR_PTR(-ENAMETOOLONG);
}

#endif // LINUX_VERSION_CODE (2.6.38)
