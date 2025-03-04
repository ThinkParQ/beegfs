#include <app/App.h>
#include <app/log/Logger.h>
#include <linux/pagemap.h>
#include "FhgfsOpsFileNative.h"
#include "FhgfsOpsFile.h"
#include "FhgfsOpsHelper.h"
#include "FhgfsOpsIoctl.h"
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <fault-inject/fault-inject.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <os/OsCompat.h>
#include <linux/aio.h>
#include <linux/kref.h>

static int writepages_init(void);
static void writepages_release(void);

static void readpages_init(void);

static ssize_t __beegfs_direct_IO(int rw, struct kiocb* iocb, struct iov_iter* iter, loff_t offset);

static struct workqueue_struct* remoting_io_queue;

bool beegfs_native_init()
{
   if(writepages_init() < 0)
      return false;

   readpages_init();

#ifndef KERNEL_HAS_ALLOC_WORKQUEUE
   remoting_io_queue = create_workqueue("beegfs/flush");
#elif defined(KERNEL_HAS_WQ_RESCUER)
   // WQ_RESCUER and WQ_MEM_RECLAIM are effectively the same thing: they ensure that
   // at least one thread to run work items on is always available.
   remoting_io_queue = alloc_workqueue("beegfs/flush", WQ_RESCUER, num_online_cpus());
#else
   remoting_io_queue = alloc_workqueue("beegfs/flush", WQ_MEM_RECLAIM, num_online_cpus());
#endif

   if(!remoting_io_queue)
      goto fail_queue;

   return true;

fail_queue:
   writepages_release();
   return false;
}

void beegfs_native_release()
{
   writepages_release();

   if(remoting_io_queue)
      destroy_workqueue(remoting_io_queue);
}



/*
 * PVRs and ARDs use the Private and Checked bits of pages to determine which is attached to a
 * page. Once we support only kernels that have the Private2 bit, we should use Private2 instead
 * of Checked.
 * Note: this is to avoid problems with 64k pages on 32 bit machines, otherwise we could use
 * low-order bits of page->private to discriminate.
 */

enum
{
   PVR_FIRST_SHIFT = 0,
   PVR_FIRST_MASK = ~PAGE_MASK << PVR_FIRST_SHIFT,

   PVR_LAST_SHIFT = PAGE_SHIFT,
   PVR_LAST_MASK = ~PAGE_MASK << PVR_LAST_SHIFT,
};

static void pvr_init(struct page* page)
{
   // pvr values *must* fit into the unsigned long private of struct page
   BUILD_BUG_ON(PVR_LAST_SHIFT + PAGE_SHIFT > 8 * sizeof(unsigned long) );

   SetPagePrivate(page);
   ClearPageChecked(page);
   page->private = 0;
}

static bool pvr_present(struct page* page)
{
   return PagePrivate(page) && !PageChecked(page);
}

static void pvr_clear(struct page* page)
{
   ClearPagePrivate(page);
}

static unsigned pvr_get_first(struct page* page)
{
   return (page->private & PVR_FIRST_MASK) >> PVR_FIRST_SHIFT;
}

static unsigned pvr_get_last(struct page* page)
{
   return (page->private & PVR_LAST_MASK) >> PVR_LAST_SHIFT;
}

static void pvr_set_first(struct page* page, unsigned first)
{
   page->private &= ~PVR_FIRST_MASK;
   page->private |= (first << PVR_FIRST_SHIFT) & PVR_FIRST_MASK;
}

static void pvr_set_last(struct page* page, unsigned last)
{
   page->private &= ~PVR_LAST_MASK;
   page->private |= (last << PVR_LAST_SHIFT) & PVR_LAST_MASK;
}

static bool pvr_can_merge(struct page* page, unsigned first, unsigned last)
{
   unsigned oldFirst = pvr_get_first(page);
   unsigned oldLast = pvr_get_last(page);

   if(first == oldLast + 1 || last + 1 == oldFirst)
      return true;

   if(oldFirst <= first && first <= oldLast)
      return true;

   if(oldFirst <= last && last <= oldLast)
      return true;

   return false;
}

static void pvr_merge(struct page* page, unsigned first, unsigned last)
{
   if(pvr_get_first(page) > first)
      pvr_set_first(page, first);

   if(pvr_get_last(page) < last)
      pvr_set_last(page, last);
}


static void beegfs_drop_all_caches(struct inode* inode)
{
   os_inode_lock(inode);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0)
   // 2.6.32 and later have truncate_pagecache, but that was incorrect
   unmap_mapping_range(inode->i_mapping, 0, 0, 1);
   truncate_inode_pages(inode->i_mapping, 0);
   unmap_mapping_range(inode->i_mapping, 0, 0, 1);
#else
   truncate_pagecache(inode, 0);
#endif

   i_size_write(inode, 0);

   os_inode_unlock(inode);
}



static int beegfs_release_range(struct file* filp, loff_t first, loff_t last)
{
   int writeRes;

   // expand range to fit full pages
   first &= PAGE_MASK;
   last |= ~PAGE_MASK;

   if (unlikely(last == -1))
   {
      printk_fhgfs(KERN_DEBUG, "range end given was -1");
      last = LLONG_MAX;
   }

   clear_bit(AS_EIO, &filp->f_mapping->flags);

   writeRes = file_write_and_wait_range(filp, first, last);
   if(writeRes < 0)
   {
      App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
      FhgfsOpsHelper_logOpMsg(3, app, file_dentry(filp), filp->f_mapping->host, __func__,
         "error %i during flush", writeRes);
      IGNORE_UNUSED_VARIABLE(app);
      return writeRes;
   }

   return 0;
}

static int beegfs_acquire_range(struct file* filp, loff_t first, loff_t last)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
   FhgfsIsizeHints iSizeHints;
   int err;

   // expand range to fit full pages
   first &= PAGE_MASK;
   last |= ~PAGE_MASK;

   if (unlikely(last == -1))
   {
      printk_fhgfs(KERN_DEBUG, "range end given was -1");
      last = LLONG_MAX; // In linux-6.2, checks for the bytes offset i.e., (end_byte < start_byte)
                        // moved from __filemap_fdatawrite_range() to the interface
                        // file[map]_write_and_wait_range() in order to provide consistent
                        // behaviour between write and wait.
   }


   err = beegfs_release_range(filp, first, last);
   if(err)
      return err;

   err = __FhgfsOps_refreshInode(app, file_inode(filp), NULL, &iSizeHints);
   if(err)
      return err;

   err = invalidate_inode_pages2_range(filp->f_mapping, first >> PAGE_SHIFT, last >> PAGE_SHIFT);
   return err;
}



static int beegfs_open(struct inode* inode, struct file* filp)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
   struct FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   int err;
   RemotingIOInfo ioInfo;
   uint64_t remoteVersion;
   bool mustAcquire;

   FhgfsOpsHelper_logOp(5, app, file_dentry(filp), file_inode(filp), __func__);
   IGNORE_UNUSED_VARIABLE(app);

   err = FhgfsOps_openReferenceHandle(app, inode, filp, filp->f_flags, NULL, &remoteVersion);
   if(err)
      return err;

   FsFileInfo_getIOInfo(__FhgfsOps_getFileInfo(filp), fhgfsInode, &ioInfo);
   if(ioInfo.pattern->chunkSize % PAGE_SIZE)
   {
      FhgfsOpsHelper_logOpMsg(1, app, file_dentry(filp), inode, __func__,
         "chunk size is not a multiple of PAGE_SIZE!");
      FhgfsOps_release(inode, filp);
      return -EIO;
   }

   FhgfsInode_entryInfoWriteLock(fhgfsInode);
   {
      mustAcquire =
         (fhgfsInode->version > remoteVersion &&
            fhgfsInode->version - remoteVersion < 0x80000000ULL) ||
         (fhgfsInode->version < remoteVersion &&
            remoteVersion - fhgfsInode->version < 0x80000000ULL);

      fhgfsInode->version = remoteVersion;
   }
   FhgfsInode_entryInfoWriteUnlock(fhgfsInode);

   if(filp->f_flags & O_APPEND)
      AtomicInt_inc(&fhgfsInode->appendFDsOpen);

   if (mustAcquire)
      err = beegfs_acquire_range(filp, 0, LLONG_MAX);

   return err;
}

static int beegfs_flush(struct file* filp, fl_owner_t id)
{
   FhgfsInode* inode = BEEGFS_INODE(file_inode(filp));
   Config* cfg = FhgfsOps_getApp(inode->vfs_inode.i_sb)->cfg;
   int result;
   FhgfsOpsErr bumpRes;
   struct FileEvent event = FILEEVENT_EMPTY;

   IGNORE_UNUSED_VARIABLE(id);

   if(filp->f_flags & O_APPEND)
      AtomicInt_dec(&BEEGFS_INODE(file_inode(filp) )->appendFDsOpen);

   /* if the file was not modified, we need not flush the caches. if we do not flush the caches,
    * we also need not bump the file version - which means that other clients can keep their
    * caches. */
   if (atomic_read(&inode->modified) == 0)
      return 0;

   /* clear the modified bit *before* any data is written out. if the inode data is modified
    * further, the flag *must* be set, even if all modifications are written out by us right
    * here. clearing the flag only after everything has been written out requires exclusion
    * between modifiers and flushers, which is prohibitively expensive. */
   atomic_set(&inode->modified, 0);

   result = beegfs_release_range(filp, 0, LLONG_MAX);
   if (result < 0)
   {
      atomic_set(&inode->modified, 1);
      return result;
   }

   if (cfg->eventLogMask & EventLogMask_FLUSH)
      FileEvent_init(&event, FileEventType_FLUSH, file_dentry(filp));

   FhgfsInode_entryInfoWriteLock(inode);

   bumpRes = FhgfsOpsRemoting_bumpFileVersion(
         FhgfsOps_getApp(file_dentry(filp)->d_sb),
         &inode->entryInfo,
         true, cfg->eventLogMask & EventLogMask_FLUSH ? &event : NULL);

   inode->version += 1;

   FhgfsInode_entryInfoWriteUnlock(inode);

   FileEvent_uninit(&event);

   if (bumpRes != FhgfsOpsErr_SUCCESS)
      atomic_set(&inode->modified, 1);

   return FhgfsOpsErr_toSysErr(bumpRes);
}

static int beegfs_release(struct inode* inode, struct file* filp)
{
   int flushRes;

   // flush entire contents of file, if this fails, a previous release operation has likely also
   // failed. the only sensible thing to do then is to drop the entire cache (because we may be
   // arbitrarily inconsistent with the rest of the world and would never know).
   flushRes = beegfs_release_range(filp, 0, LLONG_MAX);

   if(flushRes < 0)
      beegfs_drop_all_caches(inode);

   return FhgfsOps_release(inode, filp);
}

static ssize_t beegfs_file_write_iter(struct kiocb* iocb, struct iov_iter* from)
{
   return generic_file_write_iter(iocb, from);
}

static ssize_t beegfs_write_iter_direct(struct kiocb* iocb, struct iov_iter* from)
{
   iocb->ki_flags |= IOCB_DIRECT;
   return generic_file_write_iter(iocb, from);
}

static ssize_t beegfs_write_iter_locked_append(struct kiocb* iocb, struct iov_iter* from)
{
   struct file* filp = iocb->ki_filp;
   struct inode *inode = file_inode(filp);
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
   FhgfsInode *fhgfsInode = BEEGFS_INODE(inode);
   FhgfsIsizeHints iSizeHints;
   RemotingIOInfo ioInfo;
   FhgfsOpsErr ferr;
   ssize_t ret = 0;

   FsFileInfo_getIOInfo(__FhgfsOps_getFileInfo(filp), fhgfsInode, &ioInfo);

   Mutex_lock(&fhgfsInode->appendMutex);

   ferr = FhgfsOpsHelper_getAppendLock(fhgfsInode, &ioInfo);
   if (ferr)
   {
      ret = FhgfsOpsErr_toSysErr(ferr);
      goto out;
   }

   ret = __FhgfsOps_doRefreshInode(app, inode, NULL, &iSizeHints, false);

   if (! ret)
      ret = beegfs_write_iter_direct(iocb, from);

   FhgfsOpsHelper_releaseAppendLock(fhgfsInode, &ioInfo);

out:
   Mutex_unlock(&fhgfsInode->appendMutex);

   return ret;
}

static ssize_t beegfs_write_iter(struct kiocb* iocb, struct iov_iter* from)
{

   struct file* filp = iocb->ki_filp;
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   atomic_set(&BEEGFS_INODE(file_inode(filp))->modified, 1);

   if ((filp->f_flags & O_APPEND)
         && Config_getTuneUseGlobalAppendLocks(App_getConfig(app)))
      return beegfs_write_iter_locked_append(iocb, from);

   // Switch to direct (non-buffered) writes in various circumstances.
   //
   if (!iov_iter_is_pipe(from)
         && (from->count >= Config_getTuneFileCacheBufSize(App_getConfig(app))
               || BEEGFS_SHOULD_FAIL(write_force_cache_bypass, 1)))
      return beegfs_write_iter_direct(iocb, from);

   return beegfs_file_write_iter(iocb, from);
}


static ssize_t beegfs_file_read_iter(struct kiocb* iocb, struct iov_iter* to)
{
   return generic_file_read_iter(iocb, to);
}


/* like with write_iter, this is basically the O_DIRECT generic_file_read_iter. */
static ssize_t beegfs_direct_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
   struct file* filp = iocb->ki_filp;

   struct address_space* mapping = filp->f_mapping;
   struct inode* inode = mapping->host;
   size_t count = to->count;
   loff_t size;
   ssize_t result;

   if(!count)
      return 0; /* skip atime */

   size = i_size_read(inode);
   result = beegfs_release_range(filp, iocb->ki_pos, iocb->ki_pos + count - 1);
   if(!result)
   {
      struct iov_iter data = *to;
      result = __beegfs_direct_IO(READ, iocb, &data, iocb->ki_pos);
   }

   if(result > 0)
      iocb->ki_pos += result;

   if(result < 0 || to->count == result || iocb->ki_pos + result >= size)
      file_accessed(filp);

   return result;
}

static ssize_t beegfs_read_iter(struct kiocb* iocb, struct iov_iter* to)
{
   size_t size = to->count;
   struct file* filp = iocb->ki_filp;
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   FhgfsOpsHelper_logOpDebug(app, file_dentry(filp), file_inode(filp), __func__,
      "(offset: %lld; size: %zu)", iocb->ki_pos, size);
   IGNORE_UNUSED_VARIABLE(size);

   if (to->count >= Config_getTuneFileCacheBufSize(App_getConfig(app))
               || BEEGFS_SHOULD_FAIL(read_force_cache_bypass, 1))
      return beegfs_direct_read_iter(iocb, to);
   else
      return beegfs_file_read_iter(iocb, to);
}


static int __beegfs_fsync(struct file* filp, loff_t start, loff_t end, int datasync)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
   Config* cfg = App_getConfig(app);
   int err;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(file_inode(filp));

   FhgfsOpsHelper_logOp(5, app, file_dentry(filp), file_inode(filp), __func__);

   IGNORE_UNUSED_VARIABLE(start);
   IGNORE_UNUSED_VARIABLE(end);
   IGNORE_UNUSED_VARIABLE(datasync);

   /* see comment in beegfs_flush for explanation */
   atomic_set(&fhgfsInode->modified, 0);

   err = beegfs_release_range(filp, start, end);
   if(err)
   {
      atomic_set(&fhgfsInode->modified, 0);
      return err;
   }

   if(Config_getTuneRemoteFSync(cfg) )
   {
      RemotingIOInfo ioInfo;
      FhgfsOpsErr res;

      FsFileInfo_getIOInfo(__FhgfsOps_getFileInfo(filp), fhgfsInode, &ioInfo);
      res = FhgfsOpsRemoting_fsyncfile(&ioInfo, true, true, false);
      if(res != FhgfsOpsErr_SUCCESS)
      {
         atomic_set(&fhgfsInode->modified, 0);
         return FhgfsOpsErr_toSysErr(res);
      }
   }

   FhgfsInode_entryInfoWriteLock(fhgfsInode);

   err = FhgfsOpsRemoting_bumpFileVersion(
         FhgfsOps_getApp(file_dentry(filp)->d_sb),
         &fhgfsInode->entryInfo,
         true, NULL);
   if (err != FhgfsOpsErr_SUCCESS)
   {
      atomic_set(&fhgfsInode->modified, 0);
      err = FhgfsOpsErr_toSysErr(err);
   }

   fhgfsInode->version += 1;

   FhgfsInode_entryInfoWriteUnlock(fhgfsInode);

   return err;
}

#ifdef KERNEL_HAS_FSYNC_RANGE
static int beegfs_fsync(struct file* file, loff_t start, loff_t end, int datasync)
{
   return __beegfs_fsync(file, start, end, datasync);
}
#elif defined(KERNEL_HAS_FSYNC_2)
static int beegfs_fsync(struct file* file, int datasync)
{
   return __beegfs_fsync(file, 0, LLONG_MAX, datasync);
}
#else
static int beegfs_fsync(struct file* file, struct dentry* dentry, int datasync)
{
   return __beegfs_fsync(file, 0, LLONG_MAX, datasync);
}
#endif

static int beegfs_flock(struct file* filp, int cmd, struct file_lock* flock)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
   int err = -EINVAL;

   FhgfsOpsHelper_logOp(5, app, file_dentry(filp), file_inode(filp), __func__);
   IGNORE_UNUSED_VARIABLE(app);

   switch(FhgfsCommon_getFileLockType(flock))
   {
      case F_RDLCK:
      case F_WRLCK:
         err = beegfs_acquire_range(filp, 0, LLONG_MAX);
         break;

      case F_UNLCK:
         err = beegfs_release_range(filp, 0, LLONG_MAX);
         break;
   }

   if(err)
      return err;

   return FhgfsOps_flock(filp, cmd, flock);
}

static int beegfs_lock(struct file* filp, int cmd, struct file_lock* flock)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
   int err = -EINVAL;

   FhgfsOpsHelper_logOp(5, app, file_dentry(filp), file_inode(filp), __func__);
   IGNORE_UNUSED_VARIABLE(app);

   switch(FhgfsCommon_getFileLockType(flock))
   {
      case F_RDLCK:
      case F_WRLCK:
         err = beegfs_acquire_range(filp, flock->fl_start, flock->fl_end);
         break;

      case F_UNLCK:
         err = beegfs_release_range(filp, flock->fl_start, flock->fl_end);
         break;
   }

   if(err)
      return err;

   return FhgfsOps_lock(filp, cmd, flock);
}

static int beegfs_mmap(struct file* filp, struct vm_area_struct* vma)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);
   int err = -EINVAL;

   FhgfsOpsHelper_logOp(5, app, file_dentry(filp), file_inode(filp), __func__);
   IGNORE_UNUSED_VARIABLE(app);

   err = beegfs_acquire_range(filp, 0, LLONG_MAX);
   if(err)
      return err;

   err = generic_file_mmap(filp, vma);
   return err;
}

const struct file_operations fhgfs_file_native_ops = {
   .open = beegfs_open,
   .flush = beegfs_flush,
   .release = beegfs_release,
   .fsync = beegfs_fsync,
   .llseek = FhgfsOps_llseek,
   .flock = beegfs_flock,
   .lock = beegfs_lock,
   .mmap = beegfs_mmap,

   .unlocked_ioctl   = FhgfsOpsIoctl_ioctl,
#ifdef CONFIG_COMPAT
   .compat_ioctl     = FhgfsOpsIoctl_compatIoctl,
#endif

   .read_iter = beegfs_read_iter,
   .write_iter = beegfs_write_iter,

#ifdef KERNEL_HAS_GENERIC_FILE_SPLICE_READ
   .splice_read  = generic_file_splice_read,
#else
   .splice_read  = filemap_splice_read,
#endif
#if defined(KERNEL_HAS_ITER_FILE_SPLICE_WRITE)
   .splice_write = iter_file_splice_write,
#else
   .splice_write = generic_file_splice_write,
#endif
};



struct beegfs_writepages_context
{
   RemotingIOInfo ioInfo;
   struct writeback_control* wbc;
   bool unlockPages;

   // only ever written by the flusher thread
   struct beegfs_writepages_state* currentState;
   int submitted;

   SynchronizedCounter barrier;
};

static int writepages_block_size __read_mostly;

struct beegfs_writepages_state
{
   struct page** pages;
   struct kvec* kvecs;
   unsigned nr_pages;

   struct beegfs_writepages_context* context;

   struct work_struct work;
};

static mempool_t* writepages_pool;

static void* __writepages_pool_alloc(gfp_t mask, void* pool_data)
{
   struct beegfs_writepages_state* state;

   state = kmalloc(sizeof(*state), mask);
   if(!state)
      goto fail_state;

   // should use kmalloc_array, but that's not available everywhere. luckily, this will not
   // overflow.
   state->pages = kmalloc(writepages_block_size * sizeof(struct page*), mask);
   if(!state->pages)
      goto fail_pages;

   state->kvecs = kmalloc(writepages_block_size * sizeof(*state->kvecs), mask);
   if(!state->kvecs)
      goto fail_kvecs;

   return state;

fail_kvecs:
   kfree(state->pages);
fail_pages:
   kfree(state);
fail_state:
   return NULL;
}

static void __writepages_pool_free(void* element, void* pool_data)
{
   struct beegfs_writepages_state* state = element;

   if(!state)
      return;

   kfree(state->kvecs);
   kfree(state->pages);
   kfree(state);
}

static int writepages_init()
{
   // a state contains a pointer to page* array and an iovec array, so fill a page with one
   // and allocate the other to match
   writepages_block_size = PAGE_SIZE / MAX(sizeof(struct page*), sizeof(struct kvec) );

   writepages_pool = mempool_create(1, __writepages_pool_alloc, __writepages_pool_free, NULL);
   if(!writepages_pool)
      return -ENOMEM;

   return 0;
}

static void writepages_release()
{
   if(writepages_pool)
      mempool_destroy(writepages_pool);
}

static struct beegfs_writepages_state* wps_alloc(struct beegfs_writepages_context* ctx)
{
   struct beegfs_writepages_state* result = mempool_alloc(writepages_pool, GFP_NOFS);

   result->nr_pages = 0;
   result->context = ctx;

   return result;
}

static void wps_free(struct beegfs_writepages_state* state)
{
   mempool_free(state, writepages_pool);
}

static int beegfs_wps_prepare(struct beegfs_writepages_state* state, loff_t* offset, size_t* size)
{
   int i;

   *size = 0;

   if(pvr_present(state->pages[0]) )
   {
      *offset = page_offset(state->pages[0]) + pvr_get_first(state->pages[0]);

      for(i = 0; i < state->nr_pages; i++)
      {
         struct page* page = state->pages[i];
         unsigned length;

         length = pvr_get_last(page) - pvr_get_first(page) + 1;

         state->kvecs[i].iov_base = page_address(page) + pvr_get_first(page);
         state->kvecs[i].iov_len = length;

         *size += length;
      }

      return 0;
   }

   // ARDs were deleted
   BUG();
}

static void __beegfs_writepages_work(struct beegfs_writepages_state* state)
{
   int err = 0;
   loff_t offset;
   ssize_t size;
   ssize_t written = 0;

   err = beegfs_wps_prepare(state, &offset, &size);

   if(err < 0)
   {
      // Probably EIO or EDQUOT
   }
   else if(BEEGFS_SHOULD_FAIL(writepage, 1) )
   {
      // artificial write error
      err = -EIO;
   }
   else
   {
      struct iov_iter iter;
      BEEGFS_IOV_ITER_KVEC(&iter, WRITE, state->kvecs, state->nr_pages, size);

      written = FhgfsOpsRemoting_writefileVec(&iter, offset, &state->context->ioInfo, false);

      if(written < 0)
         err = FhgfsOpsErr_toSysErr(-written);
      else
         task_io_account_write(written);
   }

   size = 0;
   for(unsigned i = 0; i < state->nr_pages; i++)
   {
      struct page* page = state->pages[i];
      struct address_space *mapping = page->mapping;
      BUG_ON(! mapping);  //???

      size += state->kvecs[i].iov_len;

      if (size <= written)
      {
         pvr_clear(page);
      }
      else if (err)
      {
         mapping_set_error(mapping, err);

         pvr_clear(page);
      }
      else
      {
         // NOTE: this will cause the kernel to retry writeback at a later point
         redirty_page_for_writepage(state->context->wbc, page);
      }

      /*
      Note: As per the documentation, as of Linux 2.5.12,
      we could unlock the pages as early as after marking them using
      set_page_writeback().
      It could be that our own code requires it somewhere, though.
      */
      if(state->context->unlockPages)
         unlock_page(page);

      end_page_writeback(page);
   }
}

static void beegfs_writepages_work(struct beegfs_writepages_state* state)
{
   if(state->nr_pages > 0)
      __beegfs_writepages_work(state);

   wps_free(state);
}

static void beegfs_writepages_work_wrapper(struct work_struct* w)
{
   struct beegfs_writepages_state* state = container_of(w, struct beegfs_writepages_state, work);
   SynchronizedCounter* barrier = &state->context->barrier;

   beegfs_writepages_work(state);
   SynchronizedCounter_incCount(barrier);
}

static void beegfs_writepages_submit(struct beegfs_writepages_context* context)
{
   struct beegfs_writepages_state* state = context->currentState;

   context->submitted += 1;

   INIT_WORK(&state->work, beegfs_writepages_work_wrapper);
   queue_work(remoting_io_queue, &state->work);
}

static bool beegfs_wps_must_flush_before(struct beegfs_writepages_state* state, struct page* next)
{
   if(state->nr_pages == 0)
      return false;

   if(state->nr_pages == writepages_block_size)
      return true;

   if(state->pages[state->nr_pages - 1]->index + 1 != next->index)
      return true;

   if(pvr_present(next) )
   {
      if(pvr_get_first(next) != 0)
         return true;

      if(pvr_get_last(state->pages[state->nr_pages - 1]) != PAGE_SIZE - 1)
         return true;

      if(!pvr_present(state->pages[state->nr_pages - 1]) )
         return true;
   }

   return false;
}

#ifdef KERNEL_WRITEPAGE_HAS_FOLIO
static int beegfs_writepages_callback(struct folio *folio, struct writeback_control* wbc, void* data)
{
    struct page *page = &folio->page;
#else
static int beegfs_writepages_callback(struct page* page, struct writeback_control* wbc, void* data)
{
#endif
   struct beegfs_writepages_context* context = data;
   struct beegfs_writepages_state* state = context->currentState;

   BUG_ON(!pvr_present(page));

   if(beegfs_wps_must_flush_before(state, page) )
   {
      beegfs_writepages_submit(context);
      state = wps_alloc(context);
      context->currentState = state;
   }

   state->pages[state->nr_pages] = page;
   state->nr_pages += 1;

   //XXX can't we defer this to later?
   set_page_writeback(page);

   return 0;
}

static int beegfs_do_write_pages(struct address_space* mapping, struct writeback_control* wbc,
   struct page* page, bool unlockPages)
{
   struct inode* inode = mapping->host;
   App* app = FhgfsOps_getApp(inode->i_sb);

   FhgfsOpsErr referenceRes;
   FileHandleType handleType;
   int err;

   struct beegfs_writepages_context context = {
      .unlockPages = unlockPages,
      .wbc = wbc,
      .submitted = 0,
   };

   FhgfsOpsHelper_logOpDebug(app, NULL, inode, __func__, "page? %i %lu", page != NULL,
      page ? page->index : 0);
   IGNORE_UNUSED_VARIABLE(app);

   referenceRes = FhgfsInode_referenceHandle(BEEGFS_INODE(inode), NULL, OPENFILE_ACCESS_READWRITE,
         true, NULL, &handleType, NULL);
   if(referenceRes != FhgfsOpsErr_SUCCESS)
      return FhgfsOpsErr_toSysErr(referenceRes);

   context.currentState = wps_alloc(&context);
   SynchronizedCounter_init(&context.barrier);

   FhgfsInode_getRefIOInfo(BEEGFS_INODE(inode), handleType, OPENFILE_ACCESS_READWRITE,
      &context.ioInfo);

   FhgfsInode_incWriteBackCounter(BEEGFS_INODE(inode) );

   if(page)
   {
      #ifdef KERNEL_WRITEPAGE_HAS_FOLIO
          struct folio *folio = page_folio(page);
          err = beegfs_writepages_callback(folio, wbc, &context);
      #else
          err = beegfs_writepages_callback(page, wbc, &context);
      #endif

      //XXX not sure if it's supposed to be like that
      WARN_ON(wbc->nr_to_write != 1);
      if (! err)
         -- wbc->nr_to_write;
   }
   else
      err = write_cache_pages(mapping, wbc, beegfs_writepages_callback, &context);

   beegfs_writepages_submit(&context);

   SynchronizedCounter_waitForCount(&context.barrier, context.submitted);

   FhgfsInode_releaseHandle(BEEGFS_INODE(inode), handleType, NULL);

   FhgfsInode_decWriteBackCounter(BEEGFS_INODE(inode) );
   FhgfsInode_unsetNoIsizeDecrease(BEEGFS_INODE(inode) );

   return err;
}

static int beegfs_writepage(struct page* page, struct writeback_control* wbc)
{
   struct inode* inode = page->mapping->host;
   App* app = FhgfsOps_getApp(inode->i_sb);

   FhgfsOpsHelper_logOpDebug(app, NULL, inode, __func__, "");
   IGNORE_UNUSED_VARIABLE(app);

   return beegfs_do_write_pages(page->mapping, wbc, page, true);
}

static int beegfs_writepages(struct address_space* mapping, struct writeback_control* wbc)
{
   struct inode* inode = mapping->host;
   App* app = FhgfsOps_getApp(inode->i_sb);

   FhgfsOpsHelper_logOpDebug(app, NULL, inode, __func__, "");
   IGNORE_UNUSED_VARIABLE(app);

   return beegfs_do_write_pages(mapping, wbc, NULL, true);
}



static int beegfs_flush_page(struct page* page)
{
   struct writeback_control wbc = {
      .nr_to_write = 1,
      .sync_mode = WB_SYNC_ALL,
   };

   if(!clear_page_dirty_for_io(page) )
   {
      return 0;
   }

   return beegfs_do_write_pages(page->mapping, &wbc, page, false);
}

static int beegfs_readpage(struct file* filp, struct page* page)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   struct inode* inode = filp->f_mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(filp);
   RemotingIOInfo ioInfo;
   ssize_t readRes = -EIO;

   loff_t offset = page_offset(page);

   FhgfsOpsHelper_logOpDebug(app, file_dentry(filp), inode, __func__, "offset: %lld", offset);
   IGNORE_UNUSED_VARIABLE(app);

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   if (pvr_present(page))
   {
      readRes = beegfs_flush_page(page);
      if(readRes)
         goto out;
   }

   if(BEEGFS_SHOULD_FAIL(readpage, 1) )
      goto out;

   readRes = FhgfsOpsRemoting_readfile_kernel(page_address(page), PAGE_SIZE, offset, &ioInfo,
      fhgfsInode);

   if(readRes < 0)
   {
      readRes = FhgfsOpsErr_toSysErr(-readRes);
      goto out;
   }

   if(readRes < PAGE_SIZE)
      memset(page_address(page) + readRes, 0, PAGE_SIZE - readRes);

   readRes = 0;
   task_io_account_read(PAGE_SIZE);

out:
   page_endio(page, READ, readRes);
   return readRes;
}

#ifdef KERNEL_HAS_READ_FOLIO
static int beegfs_read_folio(struct file* filp, struct folio* folio)
{
   return beegfs_readpage(filp, &folio->page);
}
#endif

struct beegfs_readpages_context
{
   RemotingIOInfo ioInfo;
   FileHandleType handleType;
   struct inode* inode;

   struct beegfs_readpages_state* currentState;

   struct kref refs;
};

static struct beegfs_readpages_context* rpc_create(struct inode* inode)
{
   struct beegfs_readpages_context* context;
   FhgfsOpsErr referenceRes;

   context = kmalloc(sizeof(*context), GFP_NOFS);
   if(!context)
      return NULL;

   context->inode = inode;
   context->currentState = NULL;
   kref_init(&context->refs);

   referenceRes = FhgfsInode_referenceHandle(BEEGFS_INODE(inode), NULL, OPENFILE_ACCESS_READWRITE,
         true, NULL, &context->handleType, NULL);
   if(referenceRes != FhgfsOpsErr_SUCCESS)
      goto fail_reference;

   FhgfsInode_getRefIOInfo(BEEGFS_INODE(inode), context->handleType, OPENFILE_ACCESS_READWRITE,
      &context->ioInfo);

   return context;

fail_reference:
   kfree(context);
   return ERR_PTR(FhgfsOpsErr_toSysErr(referenceRes) );
}

static void __beegfs_readpages_context_free(struct kref* ref)
{
   struct beegfs_readpages_context* context;

   context = container_of(ref, struct beegfs_readpages_context, refs);

   FhgfsInode_releaseHandle(BEEGFS_INODE(context->inode), context->handleType, NULL);
   kfree(context);
}

static void rpc_get(struct beegfs_readpages_context* context)
{
   kref_get(&context->refs);
}

static void rpc_put(struct beegfs_readpages_context* context)
{
   kref_put(&context->refs, __beegfs_readpages_context_free);
}


struct beegfs_readpages_state
{
   struct page** pages;
   struct kvec *kvecs;
   unsigned nr_pages;

   struct beegfs_readpages_context* context;

   struct work_struct work;
};

static int readpages_block_size __read_mostly;

static void readpages_init()
{
   // much the same as writepages_block_size
   readpages_block_size = PAGE_SIZE / MAX(sizeof(struct page*), sizeof(struct kvec) );
}

static struct beegfs_readpages_state* rps_alloc(struct beegfs_readpages_context* context)
{
   struct beegfs_readpages_state* state;

   state = kmalloc(sizeof(*state), GFP_NOFS);
   if(!state)
      goto fail_state;

   // should use kmalloc_array, see __writepages_pool_alloc
   state->pages = kmalloc(readpages_block_size * sizeof(struct page*), GFP_NOFS);
   if(!state->pages)
      goto fail_pages;

   state->kvecs = kmalloc(readpages_block_size * sizeof(*state->kvecs), GFP_NOFS);
   if(!state->kvecs)
      goto fail_kvecs;

   state->nr_pages = 0;
   state->context = context;
   rpc_get(context);

   return state;

fail_kvecs:
   kfree(state->pages);
fail_pages:
   kfree(state);
fail_state:
   return NULL;
}

static void rps_free(struct beegfs_readpages_state* state)
{
   if(!state)
      return;

   rpc_put(state->context);
   kfree(state->kvecs);
   kfree(state->pages);
   kfree(state);
}

static void beegfs_readpages_work(struct work_struct* w)
{
   struct beegfs_readpages_state* state = container_of(w, struct beegfs_readpages_state, work);

   App* app;
   struct iov_iter iter;
   ssize_t readRes;
   unsigned validPages = 0;
   int err = 0;
   int i;

   if(!state->nr_pages)
      goto done;

   app = FhgfsOps_getApp(state->pages[0]->mapping->host->i_sb);

   FhgfsOpsHelper_logOpDebug(app, NULL, state->pages[0]->mapping->host, __func__,
      "first offset: %lld nr_pages %u", page_offset(state->pages[0]), state->nr_pages);
   IGNORE_UNUSED_VARIABLE(app);

   if(BEEGFS_SHOULD_FAIL(readpage, 1) )
   {
      err = -EIO;
      goto endio;
   }

   BEEGFS_IOV_ITER_KVEC(&iter, READ, state->kvecs, state->nr_pages,
      state->nr_pages * PAGE_SIZE);

   readRes = FhgfsOpsRemoting_readfileVec(&iter, iov_iter_count(&iter), page_offset(state->pages[0]),
      &state->context->ioInfo, BEEGFS_INODE(state->pages[0]->mapping->host) );
   if(readRes < 0)
      err = FhgfsOpsErr_toSysErr(-readRes);

   if(err < 0)
      goto endio;

   validPages = readRes / PAGE_SIZE;

   if(readRes % PAGE_SIZE != 0)
   {
      int start = readRes % PAGE_SIZE;
      memset(page_address(state->pages[validPages]) + start, 0, PAGE_SIZE - start);
      validPages += 1;
   }

endio:
   for(i = 0; i < validPages; i++)
      page_endio(state->pages[i], READ, err);

   for(i = validPages; i < state->nr_pages; i++)
   {
      ClearPageUptodate(state->pages[i]);
      unlock_page(state->pages[i]);
   }

done:
   rps_free(state);
}

static void beegfs_readpages_submit(struct beegfs_readpages_context* context)
{
   struct beegfs_readpages_state* state = context->currentState;

   INIT_WORK(&state->work, beegfs_readpages_work);
   queue_work(remoting_io_queue, &state->work);
}

static int beegfs_readpages_add_page(void* data, struct page* page)
{
   struct beegfs_readpages_context* context = data;
   struct beegfs_readpages_state* state = context->currentState;
   bool mustFlush;

   mustFlush = (state->nr_pages == readpages_block_size)
      || (state->nr_pages > 0 && state->pages[state->nr_pages - 1]->index + 1 != page->index);

   if(mustFlush)
   {
      beegfs_readpages_submit(context);
      state = rps_alloc(context);
      if(!state)
         return -ENOMEM;

      context->currentState = state;
   }

   state->pages[state->nr_pages] = page;
   state->kvecs[state->nr_pages].iov_base = page_address(page);
   state->kvecs[state->nr_pages].iov_len = PAGE_SIZE;
   state->nr_pages += 1;

   return 0;
}

#ifdef KERNEL_HAS_FOLIO
static void beegfs_readahead(struct readahead_control *ractl)
#else
static int beegfs_readpages(struct file* filp, struct address_space* mapping,
   struct list_head* pages, unsigned nr_pages)
#endif
{

#ifdef KERNEL_HAS_FOLIO
   struct inode* inode = ractl->mapping->host;
   struct file* filp = ractl->file;
   struct page* page_ra;
#else
   struct inode* inode = mapping->host;
#endif

   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(filp);
   RemotingIOInfo ioInfo;
   int err;

   struct beegfs_readpages_context* context;

   context = rpc_create(inode);
   if(IS_ERR(context) )
   #ifdef KERNEL_HAS_FOLIO
      return;
   #else
      return PTR_ERR(context);
   #endif

   context->currentState = rps_alloc(context);
   if(!context->currentState)
   {
      err = -ENOMEM;
      goto out;
   }

   #ifdef KERNEL_HAS_FOLIO
   FhgfsOpsHelper_logOpDebug(app, file_dentry(filp), inode, __func__,
         "first offset: %lld \n nr_pages %u \n no. of bytes in this readahead request: %zu\n",
         readahead_pos(ractl), readahead_count(ractl), readahead_length(ractl));
   #else
   FhgfsOpsHelper_logOpDebug(app, file_dentry(filp), inode, __func__,
         "first offset: %lld nr_pages %u", page_offset(list_entry(pages->prev, struct page, lru)),
         nr_pages);
   #endif

   IGNORE_UNUSED_VARIABLE(app);

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   #ifdef KERNEL_HAS_FOLIO
   if (readahead_count(ractl))
   {
      while ((page_ra = readahead_page(ractl)) != NULL)
      {
         err = beegfs_readpages_add_page(context, page_ra);
         put_page(page_ra);
         if (err)
            goto out;
      }
   }
   #else
   err = read_cache_pages(mapping, pages, beegfs_readpages_add_page, context);
   #endif

   beegfs_readpages_submit(context);

out:
   rpc_put(context);
   #ifdef KERNEL_HAS_FOLIO
   return;
   #else
   return err;
   #endif
}

static int __beegfs_write_begin(struct file* filp, loff_t pos, unsigned len, struct page* page)
{
   int result = 0;

   if(!pvr_present(page) )
      goto success;

   if(pvr_can_merge(page, pos & ~PAGE_MASK, (pos & ~PAGE_MASK) + len - 1))
      goto success;

   result = beegfs_flush_page(page);
   if(result)
      goto out_err;

success:
   return result;

out_err:
   unlock_page(page);
   put_page(page);
   return result;
}

static int __beegfs_write_end(struct file* filp, loff_t pos, unsigned len, unsigned copied,
   struct page* page)
{
   struct inode* inode = page->mapping->host;
   int result = copied;

   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   if(copied != len && pvr_present(page) )
   {
      FhgfsOpsHelper_logOpMsg(2, app, file_dentry(filp), inode, __func__, "short write!");
      result = 0;
      goto out;
   }

   if(i_size_read(inode) < pos + copied)
   {
      i_size_write(inode, pos + copied);
      FhgfsInode_setPageWriteFlag(BEEGFS_INODE(inode) );
      FhgfsInode_setLastWriteBackOrIsizeWriteTime(BEEGFS_INODE(inode) );
      FhgfsInode_setNoIsizeDecrease(BEEGFS_INODE(inode) );
   }

   if(pvr_present(page) )
      pvr_merge(page, pos & ~PAGE_MASK, (pos & ~PAGE_MASK) + copied - 1);
   else
   {
      pvr_init(page);
      pvr_set_first(page, pos & ~PAGE_MASK);
      pvr_set_last(page, (pos & ~PAGE_MASK) + copied - 1);
   }

out:
   ClearPageUptodate(page);

#ifdef KERNEL_HAS_FOLIO
   filemap_dirty_folio(page->mapping, page_folio(page));
#else
   __set_page_dirty_nobuffers(page);
#endif

   unlock_page(page);
   put_page(page);

   return result;
}

#ifdef KERNEL_WRITE_BEGIN_HAS_FLAGS
static int beegfs_write_begin(struct file* filp, struct address_space* mapping, loff_t pos,
   unsigned len, unsigned flags, struct page** pagep, void** fsdata)
#else
static int beegfs_write_begin(struct file* filp, struct address_space* mapping, loff_t pos,
   unsigned len, struct page** pagep, void** fsdata)
#endif
{
   pgoff_t index = pos >> PAGE_SHIFT;

#ifdef KERNEL_WRITE_BEGIN_HAS_FLAGS
   *pagep = grab_cache_page_write_begin(mapping, index, flags);
#else
   *pagep = grab_cache_page_write_begin(mapping, index);
#endif

   if(!*pagep)
      return -ENOMEM;

   return __beegfs_write_begin(filp, pos, len, *pagep);
}

static int beegfs_write_end(struct file* filp, struct address_space* mapping, loff_t pos,
   unsigned len, unsigned copied, struct page* page, void* fsdata)
{
   return __beegfs_write_end(filp, pos, len, copied, page);
}

static int beegfs_releasepage(struct page* page, gfp_t gfp)
{

   IGNORE_UNUSED_VARIABLE(gfp);

   if(pvr_present(page) )
   {
      pvr_clear(page);
      return 1;
   }

   // ARDs were deleted
   BUG();
}

#ifdef KERNEL_HAS_READ_FOLIO
static bool beegfs_release_folio(struct folio* folio, gfp_t gfp)
{
   return beegfs_releasepage(&folio->page, gfp) != 0;
}
#endif

#ifdef KERNEL_HAS_FOLIO
static bool beegfs_set_dirty_folio(struct address_space *mapping, struct folio *folio)
{
   struct page *page = &folio->page;
   if (folio_test_dirty(folio))
   {
      printk_fhgfs_debug(KERN_INFO,"%s %p dirty_folio %p idx %lu -- already dirty\n", __func__,
         mapping->host, folio, folio->index);
      VM_BUG_ON_FOLIO(!folio_test_private(folio), folio);
      return false;
   }

#else
static int beegfs_set_page_dirty(struct page* page)
{
#endif

   atomic_set(&BEEGFS_INODE(page->mapping->host)->modified, 1);

   pvr_init(page);
   pvr_set_first(page, 0);
   pvr_set_last(page, PAGE_SIZE - 1);

#ifdef KERNEL_HAS_FOLIO
   return filemap_dirty_folio(mapping,folio);
#else
   return __set_page_dirty_nobuffers(page);
#endif
}

static void __beegfs_invalidate_page(struct page* page, unsigned begin, unsigned end)
{
   if(pvr_present(page) )
   {
      unsigned pvr_begin = pvr_get_first(page);
      unsigned pvr_end = pvr_get_last(page);

      if(begin == 0 && end == PAGE_SIZE)
      {
         pvr_clear(page);
         ClearPageUptodate(page);
         return;
      }

      if(begin < pvr_begin)
         pvr_set_first(page, begin);

      if(pvr_end < end)
         pvr_set_last(page, end);

      return;
   }

   // ARDs were deleted
   BUG();
}

#if defined(KERNEL_HAS_FOLIO)
static void beegfs_invalidate_folio(struct folio *folio, size_t offset, size_t length)
{
   if (offset != 0 || length < folio_size(folio))
      return;

   //FIX ME: If this folio_wait_writeback(folio) makes sense here (imp: as per doc)
   __beegfs_invalidate_page(&folio->page, offset, (offset+length));

}

#elif !defined(KERNEL_HAS_INVALIDATEPAGE_RANGE)
static void beegfs_invalidate_page(struct page* page, unsigned long begin)
{
   __beegfs_invalidate_page(page, begin, PAGE_CACHE_SIZE);
}
#else
static void beegfs_invalidate_page(struct page* page, unsigned begin, unsigned end)
{
   __beegfs_invalidate_page(page, begin, end);
}
#endif

static ssize_t beegfs_dIO_read(struct kiocb* iocb, struct iov_iter* iter, loff_t offset,
   RemotingIOInfo* ioInfo)
{
   struct file* filp = iocb->ki_filp;
   struct inode* inode = file_inode(filp);
   struct dentry* dentry = file_dentry(filp);

   App* app = FhgfsOps_getApp(dentry->d_sb);

   ssize_t result = 0;

   FhgfsOpsHelper_logOpDebug(app, dentry, inode, __func__, "pos: %lld, nr_segs: %lld",
      offset, beegfs_iov_iter_nr_segs(iter));
   IGNORE_UNUSED_VARIABLE(app);

   result = FhgfsOpsRemoting_readfileVec(iter, iov_iter_count(iter), offset, ioInfo, BEEGFS_INODE(inode));

   if(result < 0)
      return FhgfsOpsErr_toSysErr(-result);

   offset += result;

   if(iov_iter_count(iter) > 0)
   {
      ssize_t readRes = __FhgfsOps_readSparse(filp, iter, iov_iter_count(iter), offset + result);

      result += readRes;
   }

   task_io_account_read(result);

   if(offset > i_size_read(inode) )
      i_size_write(inode, offset);

   return result;
}

static ssize_t beegfs_dIO_write(struct kiocb* iocb, struct iov_iter* iter, loff_t offset,
   RemotingIOInfo* ioInfo)
{
   struct file* filp = iocb->ki_filp;
   struct inode* inode = file_inode(filp);
   struct dentry* dentry = file_dentry(filp);

   App* app = FhgfsOps_getApp(dentry->d_sb);

   ssize_t result = 0;

   FhgfsOpsHelper_logOpDebug(app, dentry, inode, __func__, "pos: %lld, nr_segs: %lld",
      offset, beegfs_iov_iter_nr_segs(iter));
   IGNORE_UNUSED_VARIABLE(app);
   IGNORE_UNUSED_VARIABLE(inode);

   result = FhgfsOpsRemoting_writefileVec(iter, offset, ioInfo, false);

   if(result < 0)
      return FhgfsOpsErr_toSysErr(-result);

   offset += result;

   task_io_account_write(result);

   return result;
}

static ssize_t __beegfs_direct_IO(int rw, struct kiocb* iocb, struct iov_iter* iter, loff_t offset)
{
   struct file* filp = iocb->ki_filp;
   struct inode* inode = file_inode(filp);
   RemotingIOInfo ioInfo;

   FsFileInfo_getIOInfo(__FhgfsOps_getFileInfo(filp), BEEGFS_INODE(inode), &ioInfo);

   {
      ssize_t result;

      switch(rw)
      {
         case READ:
            result = beegfs_dIO_read(iocb, iter, offset, &ioInfo);
            break;

         case WRITE:
            result = beegfs_dIO_write(iocb, iter, offset, &ioInfo);
            break;

         default:
            BUG();
            return -EINVAL;
      }

      return result;
   }
}

static ssize_t beegfs_direct_IO(struct kiocb* iocb, struct iov_iter* iter)
{
   return __beegfs_direct_IO(iov_iter_rw(iter), iocb, iter, iocb->ki_pos);
}

#ifdef KERNEL_HAS_FOLIO
static int beegfs_launder_folio(struct folio *folio)
{
   return beegfs_flush_page(&folio->page);
}
#else
static int beegfs_launderpage(struct page* page)
{
   return beegfs_flush_page(page);
}
#endif

const struct address_space_operations fhgfs_addrspace_native_ops = {
#ifdef KERNEL_HAS_READ_FOLIO
   .read_folio = beegfs_read_folio,
   .release_folio = beegfs_release_folio,
#else
   .readpage = beegfs_readpage,
   .releasepage = beegfs_releasepage,
#endif

   .writepage = beegfs_writepage,
   .direct_IO = beegfs_direct_IO,

#ifdef KERNEL_HAS_FOLIO
   .readahead = beegfs_readahead,
   .dirty_folio = beegfs_set_dirty_folio,
   .invalidate_folio = beegfs_invalidate_folio,
   .launder_folio = beegfs_launder_folio,
#else
   .readpages = beegfs_readpages,
   .set_page_dirty = beegfs_set_page_dirty,
   .invalidatepage = beegfs_invalidate_page,
   .launder_page = beegfs_launderpage,
#endif

   .writepages = beegfs_writepages,
   .write_begin = beegfs_write_begin,
   .write_end = beegfs_write_end,
};
