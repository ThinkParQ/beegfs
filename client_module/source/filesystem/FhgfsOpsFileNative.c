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



static bool is_pipe_iter(struct iov_iter* iter)
{
#ifdef KERNEL_HAS_ITER_PIPE
      return iter->type & ITER_PIPE;
#else
      return false;
#endif
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



struct append_range_descriptor
{
   unsigned long firstPageIndex;

   loff_t fileOffset;
   loff_t firstPageStart;

   size_t size;
   size_t dataPresent;

   struct mutex mtx;

   struct kref refs;
};

static struct append_range_descriptor* ard_create(size_t size)
{
   struct append_range_descriptor* ard;

   ard = kmalloc(sizeof(*ard), GFP_NOFS);
   if(!ard)
      return NULL;

   ard->fileOffset = -1;
   ard->firstPageIndex = 0;
   ard->firstPageStart = -1;

   ard->size = size;
   ard->dataPresent = 0;

   mutex_init(&ard->mtx);

   kref_init(&ard->refs);

   return ard;
}

static void ard_kref_release(struct kref* kref)
{
   struct append_range_descriptor* ard = container_of(kref, struct append_range_descriptor, refs);

   mutex_destroy(&ard->mtx);
   kfree(ard);
}

static void ard_get(struct append_range_descriptor* ard)
{
   kref_get(&ard->refs);
}

static int ard_put(struct append_range_descriptor* ard)
{
   return kref_put(&ard->refs, ard_kref_release);
}

static struct append_range_descriptor* ard_from_page(struct page* page)
{
   if(!PagePrivate(page) || !PageChecked(page) )
      return NULL;

   return (struct append_range_descriptor*) page->private;
}

static struct append_range_descriptor* __mapping_ard(struct address_space* mapping)
{
#ifdef KERNEL_HAS_ADDRSPACE_ASSOC_MAPPING
   // guarantees that pointer casting round-trips properly. gotta love C
   BUILD_BUG_ON(__alignof__(struct append_range_descriptor) < __alignof(struct address_space) );
   // same field, same semantics, different type.
   return (struct append_range_descriptor*) mapping->assoc_mapping;
#else
   return mapping->private_data;
#endif
}

static struct append_range_descriptor* mapping_ard_get(struct address_space* mapping)
{
   struct append_range_descriptor* ard;

   spin_lock(&mapping->private_lock);
   {
      ard = __mapping_ard(mapping);
      if(ard)
         ard_get(ard);
   }
   spin_unlock(&mapping->private_lock);

   return ard;
}

static void __set_mapping_ard(struct address_space* mapping, struct append_range_descriptor* ard)
{
#ifdef KERNEL_HAS_ADDRSPACE_ASSOC_MAPPING
   mapping->assoc_mapping = (struct address_space*) ard;
#else
   mapping->private_data = ard;
#endif
}

static void set_mapping_ard(struct address_space* mapping, struct append_range_descriptor* ard)
{
   struct append_range_descriptor* oldArd;

   if (ard)
      ard_get(ard);

   spin_lock(&mapping->private_lock);
   oldArd = __mapping_ard(mapping);
   __set_mapping_ard(mapping, ard);
   spin_unlock(&mapping->private_lock);

   if (oldArd)
      ard_put(oldArd);
}

static void ard_assign(struct page* page, struct append_range_descriptor* ard)
{
   if(!ard)
   {
      ard = ard_from_page(page);

      BUG_ON(!ard);

      ard_put(ard);
      ClearPagePrivate(page);
      ClearPageChecked(page);
      page->private = 0;
      return;
   }

   BUG_ON(ard_from_page(page) );

   ard_get(ard);
   SetPagePrivate(page);
   SetPageChecked(page);
   page->private = (unsigned long) ard;
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
   first &= ~PAGE_MASK;
   last |= PAGE_MASK;

   clear_bit(AS_EIO, &filp->f_mapping->flags);

   writeRes = filemap_write_and_wait_range(filp->f_mapping, first, last);
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
   first &= ~PAGE_MASK;
   last |= PAGE_MASK;

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
      err = beegfs_acquire_range(filp, 0, -1);

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

   result = beegfs_release_range(filp, 0, -1);
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
   flushRes = beegfs_release_range(filp, 0, -1);

   if(flushRes < 0)
      beegfs_drop_all_caches(inode);

   return FhgfsOps_release(inode, filp);
}

#if defined(KERNEL_HAS_AIO_WRITE_BUF)
static ssize_t beegfs_file_write_iter(struct kiocb* iocb, struct iov_iter* from)
{
   struct iovec iov = iov_iter_iovec(from);

   return generic_file_aio_write(iocb, iov.iov_base, iov.iov_len, iocb->ki_pos);
}
#elif !defined(KERNEL_HAS_WRITE_ITER)
static ssize_t beegfs_file_write_iter(struct kiocb* iocb, struct iov_iter* from)
{
   struct iovec iov = iov_iter_iovec(from);

   if (from->iov_offset == 0)
      return generic_file_aio_write(iocb, from->iov, from->nr_segs, iocb->ki_pos);
   else
      return generic_file_aio_write(iocb, &iov, 1, iocb->ki_pos);
}
#else
static ssize_t beegfs_file_write_iter(struct kiocb* iocb, struct iov_iter* from)
{
   return generic_file_write_iter(iocb, from);
}
#endif


static struct append_range_descriptor* beegfs_get_or_create_ard(struct address_space* mapping,
   loff_t begin, size_t newData)
{
   struct append_range_descriptor* ard;

   ard = mapping_ard_get(mapping);

   if(!ard)
      goto no_ard;

   mutex_lock(&ard->mtx);

   if(ard->fileOffset >= 0)
      goto replace_ard;

   if(ard->firstPageIndex * PAGE_SIZE + ard->firstPageStart + ard->size < begin)
      goto replace_ard;

   ard->size += newData;

   mutex_unlock(&ard->mtx);
   return ard;

replace_ard:
   ard_put(ard);
   set_mapping_ard(mapping, NULL);
   mutex_unlock(&ard->mtx);

no_ard:
   ard = ard_create(newData);
   if(!ard)
      return ERR_PTR(-ENOMEM);

   set_mapping_ard(mapping, ard);
   return ard;
}

static ssize_t beegfs_append_iter(struct kiocb* iocb, struct iov_iter* from)
{
   struct file* filp = iocb->ki_filp;
   struct FhgfsInode* inode = BEEGFS_INODE(filp->f_mapping->host);
   size_t size = from->count;

   struct append_range_descriptor* ard;
   ssize_t written;

   Mutex_lock(&inode->appendMutex);
   do {
      /* if the append is too large, we may overflow the page cache. make sure concurrent
       * flushers will allocate the correct size on the storage servers if nothing goes wrong
       * during the actual append write. if errors do happen, we will produce a hole in the
       * file. */
      ard = beegfs_get_or_create_ard(inode->vfs_inode.i_mapping, iocb->ki_pos, size);
      if(IS_ERR(ard) )
      {
         written = PTR_ERR(ard);
         break;
      }

      written = beegfs_file_write_iter(iocb, from);

      mutex_lock(&ard->mtx);
      os_inode_lock(&inode->vfs_inode);
      do {
         if(ard->fileOffset >= 0)
         {
            set_mapping_ard(inode->vfs_inode.i_mapping, NULL);
            ard->dataPresent += MAX(written, 0);
            i_size_write(&inode->vfs_inode, i_size_read(&inode->vfs_inode) + size);
            break;
         }

         if(written < 0)
            ard->size -= size;
         else
         {
            ard->size -= size - written;
            i_size_write(&inode->vfs_inode, iocb->ki_pos);
            ard->dataPresent += written;
         }

         if(ard->size == 0)
            set_mapping_ard(inode->vfs_inode.i_mapping, NULL);
      } while (0);
      os_inode_unlock(&inode->vfs_inode);
      mutex_unlock(&ard->mtx);

      ard_put(ard);
   } while (0);
   Mutex_unlock(&inode->appendMutex);

   return written;
}

static ssize_t beegfs_write_iter(struct kiocb* iocb, struct iov_iter* from)
{

   struct file* filp = iocb->ki_filp;
   size_t size = from->count;

   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   FhgfsOpsHelper_logOpDebug(app, file_dentry(filp), file_inode(filp), __func__,
      "(offset: %lld; size: %zu)", iocb->ki_pos, size);
   IGNORE_UNUSED_VARIABLE(app);
   IGNORE_UNUSED_VARIABLE(size);

   atomic_set(&BEEGFS_INODE(file_inode(filp))->modified, 1);

   if(filp->f_flags & O_APPEND)
      return beegfs_append_iter(iocb, from);

   if (!is_pipe_iter(from)
         && (from->count >= Config_getTuneFileCacheBufSize(App_getConfig(app))
               || BEEGFS_SHOULD_FAIL(write_force_cache_bypass, 1)))
   {
      ssize_t result;

#ifdef KERNEL_HAS_IOCB_DIRECT
      iocb->ki_flags |= IOCB_DIRECT;
      result = generic_file_write_iter(iocb, from);
#else
      /* we have to do basically all of generic_file_write_iter by foot here, because only
       * new kernels provide an iocb flag to force direct IO, which would do the right thing. */
      size_t size = from->count;
      loff_t pos = iocb->ki_pos;
      struct inode* inode = file_inode(filp);

      os_inode_lock(inode);
      do {
         result = os_generic_write_checks(filp, &pos, &size, S_ISBLK(inode->i_mode) );
         if(result)
            break;

         iov_iter_truncate(from, size);

#ifdef KERNEL_HAS_FILE_REMOVE_SUID
         result = file_remove_suid(filp);
#elif defined(KERNEL_HAS_FILE_REMOVE_PRIVS)
         result = file_remove_privs(filp);
#else
         result = remove_suid(file_dentry(filp));
#endif
         if(result)
            break;

#ifndef KERNEL_HAS_WRITE_ITER
         /* if we ever *do* get an iterator with a non-zero iov_offset, warn and fail. we should
          * never get one of these, because iov_iter appeared in 3.16. anything that does create
          * such an iterator must have originated in this file.
          */
         if (unlikely(from->iov_offset > 0))
         {
            WARN_ON(1);
            result = -EINVAL;
         }
         else
         {
            result = generic_file_direct_write(iocb, from->iov, &from->nr_segs, pos,
            #ifdef KERNEL_HAS_GENERIC_FILE_DIRECT_WRITE_POSP
               &iocb->ki_pos,
            #endif
               size, size);
         }
#else
         result = generic_file_direct_write(iocb, from, pos);
#endif
      } while(0);
      os_inode_unlock(inode);
#endif

      return result;
   }

   return beegfs_file_write_iter(iocb, from);
}

#if !defined(KERNEL_HAS_WRITE_ITER)
static ssize_t beegfs_aio_write_iov(struct kiocb* iocb, const struct iovec* iov,
   unsigned long nr_segs, loff_t pos)
{
   struct iov_iter iter;
   size_t count = iov_length(iov, nr_segs);

   BEEGFS_IOV_ITER_INIT(&iter, WRITE, iov, nr_segs, count);

   return beegfs_write_iter(iocb, &iter);
}

#if defined(KERNEL_HAS_AIO_WRITE_BUF)
static ssize_t beegfs_aio_write(struct kiocb* iocb, const char __user* buf, size_t count,
   loff_t pos)
{
   struct iovec iov = {
      .iov_base = (char*) buf,
      .iov_len = count,
   };

   return beegfs_aio_write_iov(iocb, &iov, 1, pos);
}
#else
static ssize_t beegfs_aio_write(struct kiocb* iocb, const struct iovec* iov,
   unsigned long nr_segs, loff_t pos)
{
   return beegfs_aio_write_iov(iocb, iov, nr_segs, pos);
}
#endif
#endif


#ifdef KERNEL_HAS_AIO_WRITE_BUF
static ssize_t beegfs_file_read_iter(struct kiocb* iocb, struct iov_iter* from)
{
   struct iovec iov = iov_iter_iovec(from);

   return generic_file_aio_read(iocb, iov.iov_base, iov.iov_len, iocb->ki_pos);
}
#elif !defined(KERNEL_HAS_WRITE_ITER)
static ssize_t beegfs_file_read_iter(struct kiocb* iocb, struct iov_iter* from)
{
   return generic_file_aio_read(iocb, from->iov, from->nr_segs, iocb->ki_pos);
}
#else
static ssize_t beegfs_file_read_iter(struct kiocb* iocb, struct iov_iter* from)
{
   return generic_file_read_iter(iocb, from);
}
#endif

static ssize_t beegfs_read_iter(struct kiocb* iocb, struct iov_iter* to)
{
   struct file* filp = iocb->ki_filp;
   size_t size = to->count;

   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   FhgfsOpsHelper_logOpDebug(app, file_dentry(filp), file_inode(filp), __func__,
      "(offset: %lld; size: %zu)", iocb->ki_pos, size);
   IGNORE_UNUSED_VARIABLE(app);
   IGNORE_UNUSED_VARIABLE(size);

   if (!is_pipe_iter(to)
         && (to->count >= Config_getTuneFileCacheBufSize(App_getConfig(app))
               || BEEGFS_SHOULD_FAIL(read_force_cache_bypass, 1)))
   {
      /* like with write_iter, this is basically the O_DIRECT generic_file_read_iter. */
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

   return beegfs_file_read_iter(iocb, to);
}

#ifndef KERNEL_HAS_WRITE_ITER
static ssize_t beegfs_aio_read_iov(struct kiocb* iocb, const struct iovec* iov,
   unsigned long nr_segs, loff_t pos)
{
   struct iov_iter iter;
   size_t count = iov_length(iov, nr_segs);

   BEEGFS_IOV_ITER_INIT(&iter, READ, iov, nr_segs, count);

   return beegfs_read_iter(iocb, &iter);
}

#ifdef KERNEL_HAS_AIO_WRITE_BUF
static ssize_t beegfs_aio_read(struct kiocb* iocb, char __user* buf, size_t count,
   loff_t pos)
{
   struct iovec iov = {
      .iov_base = buf,
      .iov_len = count,
   };

   return beegfs_aio_read_iov(iocb, &iov, 1, pos);
}
#else
static ssize_t beegfs_aio_read(struct kiocb* iocb, const struct iovec* iov,
   unsigned long nr_segs, loff_t pos)
{
   return beegfs_aio_read_iov(iocb, iov, nr_segs, pos);
}
#endif
#endif


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

   switch(flock->fl_type)
   {
      case F_RDLCK:
      case F_WRLCK:
         err = beegfs_acquire_range(filp, 0, -1);
         break;

      case F_UNLCK:
         err = beegfs_release_range(filp, 0, -1);
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

   switch(flock->fl_type)
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

   err = beegfs_acquire_range(filp, 0, -1);
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

#if !defined(KERNEL_HAS_WRITE_ITER)
   .read = do_sync_read,
   .write = do_sync_write,
   .aio_read = beegfs_aio_read,
   .aio_write = beegfs_aio_write,
#else
   .read_iter = beegfs_read_iter,
   .write_iter = beegfs_write_iter,
#endif

#if defined(KERNEL_HAS_WRITE_ITER)
   .splice_read  = generic_file_splice_read,
   .splice_write = iter_file_splice_write,
#else
   .splice_read  = generic_file_splice_read,
   .splice_write = generic_file_splice_write,
#endif
};



struct beegfs_writepages_context
{
   RemotingIOInfo ioInfo;
   struct writeback_control* wbc;
   bool unlockPages;
   bool appendLockTaken;

   // only ever written by the flusher thread
   struct beegfs_writepages_state* currentState;
   int submitted;

   SynchronizedCounter barrier;
};

static int writepages_block_size __read_mostly;

struct beegfs_writepages_state
{
   struct page** pages;
   struct iovec* iov;
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

   state->iov = kmalloc(writepages_block_size * sizeof(struct iovec), mask);
   if(!state->iov)
      goto fail_iov;

   return state;

fail_iov:
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

   kfree(state->iov);
   kfree(state->pages);
   kfree(state);
}

static int writepages_init()
{
   // a state contains a pointer to page* array and an iovec array, so fill a page with one
   // and allocate the other to match
   writepages_block_size = PAGE_SIZE / MAX(sizeof(struct page*), sizeof(struct iovec) );

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

static int beegfs_allocate_ard(struct beegfs_writepages_state* state)
{
   struct append_range_descriptor* ard = ard_from_page(state->pages[0]);
   struct address_space* mapping = state->pages[0]->mapping;
   App* app = FhgfsOps_getApp(mapping->host->i_sb);

   int err = 0;

   mutex_lock(&ard->mtx);
   if(ard->fileOffset >= 0)
   {
      mutex_unlock(&ard->mtx);
      return 0;
   }

   if(!Config_getTuneUseGlobalAppendLocks(App_getConfig(app) ) )
   {
      char zero = 0;
      ssize_t appendRes;
      struct iovec reserveIov = {
         .iov_len = 1,
         .iov_base = &zero,
      };

      appendRes = FhgfsOpsHelper_appendfileVecOffset(BEEGFS_INODE(mapping->host), &reserveIov, 1,
         &state->context->ioInfo, ard->size - 1, &ard->fileOffset);
      if (appendRes >= 0)
         ard->fileOffset -= ard->size;
      else
         err = appendRes;
   }
   else
   if(!state->context->appendLockTaken)
   {
      FhgfsOpsErr lockRes;
      FhgfsOpsErr statRes;
      fhgfs_stat stat;
      FhgfsInode* inode = BEEGFS_INODE(mapping->host);

      lockRes = FhgfsOpsHelper_getAppendLock(inode, &state->context->ioInfo);
      if(lockRes)
      {
         err = -lockRes;
         goto error;
      }

      /* when global append locks are used, this will run in the writeback thread, not in a work
       * queue, so no synchronization is needed. */
      state->context->appendLockTaken = true;

      FhgfsInode_entryInfoReadLock(inode); // LOCK EntryInfo
      statRes = FhgfsOpsRemoting_statDirect(app, FhgfsInode_getEntryInfo(inode), &stat);
      FhgfsInode_entryInfoReadUnlock(inode); // UNLOCK EntryInfo

      if(unlikely(statRes != FhgfsOpsErr_SUCCESS) )
      {
         err = -statRes;
         goto error;
      }

      ard->fileOffset = stat.size;
   }

   if (err < 0)
      err = FhgfsOpsErr_toSysErr(-err);

error:
   mutex_unlock(&ard->mtx);
   return err;
}

static int beegfs_wps_prepare(struct beegfs_writepages_state* state, loff_t* offset, size_t* size)
{
   struct append_range_descriptor* ard;
   int err;
   int i;
   loff_t appendEnd;

   *size = 0;

   if(pvr_present(state->pages[0]) )
   {
      loff_t isize = i_size_read(state->pages[0]->mapping->host);

      *offset = page_offset(state->pages[0]) + pvr_get_first(state->pages[0]);

      for(i = 0; i < state->nr_pages; i++)
      {
         struct page* page = state->pages[i];
         unsigned length;

         if(page_offset(page) + pvr_get_last(page) >= isize)
            length = isize - page_offset(page) - pvr_get_first(page);
         else
            length = pvr_get_last(page) - pvr_get_first(page) + 1;

         state->iov[i].iov_base = page_address(page) + pvr_get_first(page);
         state->iov[i].iov_len = length;

         *size += length;
      }

      return 0;
   }

   ard = ard_from_page(state->pages[0]);
   BUG_ON(!ard);

   err = beegfs_allocate_ard(state);
   if(err < 0)
      return err;

   *offset = ard->fileOffset;
   if (state->pages[0]->index != ard->firstPageIndex)
      *offset += (state->pages[0]->index - ard->firstPageIndex) * PAGE_SIZE - ard->firstPageStart;

   appendEnd = ard->firstPageIndex * PAGE_SIZE + ard->firstPageStart + ard->dataPresent;

   for(i = 0; i < state->nr_pages; i++)
   {
      struct page* page = state->pages[i];
      loff_t offsetInPage;

      if(page->index == ard->firstPageIndex)
         offsetInPage = ard->firstPageStart;
      else
         offsetInPage = 0;

      state->iov[i].iov_base = page_address(page) + offsetInPage;
      state->iov[i].iov_len = min_t(size_t, appendEnd - (page_offset(page) + offsetInPage),
            PAGE_SIZE - offsetInPage);

      *size += state->iov[i].iov_len;
   }

   return 0;
}

static int beegfs_writepages_work(struct beegfs_writepages_state* state)
{
   App* app;
   struct iov_iter iter;
   unsigned i;
   int result = 0;
   ssize_t written;
   loff_t offset;
   size_t size;

   if(state->nr_pages == 0)
      goto free;

   app = FhgfsOps_getApp(state->pages[0]->mapping->host->i_sb);

   FhgfsOpsHelper_logOpDebug(app, NULL, state->pages[0]->mapping->host, __func__,
      "first offset: %lld nr_pages %u", page_offset(state->pages[0]), state->nr_pages);
   IGNORE_UNUSED_VARIABLE(app);

   result = beegfs_wps_prepare(state, &offset, &size);
   if(result < 0)
      goto writes_done;

   if(BEEGFS_SHOULD_FAIL(writepage, 1) )
   {
      result = -EIO;
      goto artifical_write_error;
   }

   BEEGFS_IOV_ITER_INIT(&iter, WRITE, state->iov, state->nr_pages, size);
   written = FhgfsOpsRemoting_writefileVec(&iter, offset, &state->context->ioInfo,
      state->context->appendLockTaken);

   if(written < 0)
   {
      result = FhgfsOpsErr_toSysErr(-written);

artifical_write_error:
      for(i = 0; i < state->nr_pages; i++)
      {
         page_endio(state->pages[i], WRITE, result);
         redirty_page_for_writepage(state->context->wbc, state->pages[i]);
      }

      goto out;
   }

   task_io_account_write(written);
   size = written;

writes_done:
   for(i = 0; i < state->nr_pages; i++)
   {
      struct page* page = state->pages[i];

      if(size >= state->iov[i].iov_len)
      {
         size -= state->iov[i].iov_len;

         if(pvr_present(page) )
            pvr_clear(page);
         else
            ard_assign(page, NULL);

         page_endio(page, WRITE, 0);
      }
      else
      {
         // can't decipher *what* exactly happened, but it was a short write and thus bad
         result = -EIO;
         redirty_page_for_writepage(state->context->wbc, page);
      }
   }

out:
   if(state->context->unlockPages)
   {
      for(i = 0; i < state->nr_pages; i++)
         unlock_page(state->pages[i]);
   }

free:
   wps_free(state);
   return result;
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
   Config* config = App_getConfig(context->ioInfo.app);

   if(state->nr_pages && ard_from_page(state->pages[0]) &&
      Config_getTuneUseGlobalAppendLocks(config) )
   {
      /* execute globally locked appends from the writeback thread to ensure correct write ordering
       * on the storage servers. PVR flushes can still run from the workqueue because they cannot
       * intersect any ARD on the same file and client. */
      beegfs_writepages_work(state);
      return;
   }

   context->submitted += 1;

   INIT_WORK(&state->work, beegfs_writepages_work_wrapper);
   queue_work(remoting_io_queue, &state->work);
}

static bool beegfs_wps_must_flush_before(struct beegfs_writepages_state* state, struct page* next)
{
   struct append_range_descriptor* ard;

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

   ard = ard_from_page(next);
   if(ard)
   {
      if(ard_from_page(state->pages[state->nr_pages - 1]) != ard)
         return true;
   }

   return false;
}

static int beegfs_writepages_callback(struct page* page, struct writeback_control* wbc, void* data)
{
   struct beegfs_writepages_context* context = data;
   struct beegfs_writepages_state* state = context->currentState;

   BUG_ON(!pvr_present(page) && !ard_from_page(page) );

   if(beegfs_wps_must_flush_before(state, page) )
   {
      beegfs_writepages_submit(context);
      state = wps_alloc(context);
      context->currentState = state;
   }

   state->pages[state->nr_pages] = page;
   state->nr_pages += 1;
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
      .appendLockTaken = false,
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

   if(Config_getTuneUseGlobalAppendLocks(App_getConfig(app) ) )
   {
      /* ensure that the active ARD is written out end to end when global append locks are used,
       * to make sure that tail(1)-like readers never read blocks of zeroes from reserved-unwritten
       * ranges of the file. since it is unlikely that files opened with O_APPEND will have
       * significant PVR write traffic, just sync the whole file.
       *
       * we cannot simply flush the ARD attached to the first page of the range (if present),
       * because there may be an ARD lurking somewhere in the entire range that must not be flushed
       * only partially.
       *
       * if we were only supposed to write out an ARD page (e.g. when an ARD page is modified
       * by non-append writes), also sync the whole file.
       *
       * PVR pages are safe for now. inodes that have not had O_APPEND writes are also safe from
       * special treatment. we approximate "has O_APPEND writes" by "has O_APPEND handles". that's
       * not perfect, but close enough for most cases. */
      if( (page && ard_from_page(page) ) ||
          (!page && AtomicInt_read(&BEEGFS_INODE(inode)->appendFDsOpen) ) )
      {
         wbc->range_cyclic = 0;
         wbc->range_start = 0;
         wbc->range_end = LLONG_MAX;
         wbc->nr_to_write = LONG_MAX;
         wbc->sync_mode = WB_SYNC_ALL;
      }
   }

   if(page)
      err = beegfs_writepages_callback(page, wbc, &context);
   else
      err = write_cache_pages(mapping, wbc, beegfs_writepages_callback, &context);

   beegfs_writepages_submit(&context);

   SynchronizedCounter_waitForCount(&context.barrier, context.submitted);

   if(context.appendLockTaken)
      FhgfsOpsHelper_releaseAppendLock(BEEGFS_INODE(inode), &context.ioInfo);

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
      return 0;

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

   if(pvr_present(page) || ard_from_page(page) )
   {
      readRes = beegfs_flush_page(page);
      if(readRes)
         goto out;
   }

   if(BEEGFS_SHOULD_FAIL(readpage, 1) )
      goto out;

   readRes = FhgfsOpsRemoting_readfile(page_address(page), PAGE_SIZE, offset, &ioInfo,
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
   struct iovec* iov;
   unsigned nr_pages;

   struct beegfs_readpages_context* context;

   struct work_struct work;
};

static int readpages_block_size __read_mostly;

static void readpages_init()
{
   // much the same as writepages_block_size
   readpages_block_size = PAGE_SIZE / MAX(sizeof(struct page*), sizeof(struct iovec) );
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

   state->iov = kmalloc(readpages_block_size * sizeof(struct iovec), GFP_NOFS);
   if(!state->iov)
      goto fail_iov;

   state->nr_pages = 0;
   state->context = context;
   rpc_get(context);

   return state;

fail_iov:
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
   kfree(state->iov);
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

   BEEGFS_IOV_ITER_INIT(&iter, READ, state->iov, state->nr_pages,
      state->nr_pages * PAGE_SIZE);

   readRes = FhgfsOpsRemoting_readfileVec(&iter, page_offset(state->pages[0]),
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
   state->iov[state->nr_pages].iov_base = page_address(page);
   state->iov[state->nr_pages].iov_len = PAGE_SIZE;
   state->nr_pages += 1;

   return 0;
}

static int beegfs_readpages(struct file* filp, struct address_space* mapping,
   struct list_head* pages, unsigned nr_pages)
{
   App* app = FhgfsOps_getApp(file_dentry(filp)->d_sb);

   struct inode* inode = mapping->host;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   FsFileInfo* fileInfo = __FhgfsOps_getFileInfo(filp);
   RemotingIOInfo ioInfo;
   int err;

   struct beegfs_readpages_context* context;

   context = rpc_create(inode);
   if(IS_ERR(context) )
      return PTR_ERR(context);

   context->currentState = rps_alloc(context);
   if(!context->currentState)
   {
      err = -ENOMEM;
      goto out;
   }

   FhgfsOpsHelper_logOpDebug(app, file_dentry(filp), inode, __func__,
         "first offset: %lld nr_pages %u", page_offset(list_entry(pages->prev, struct page, lru)),
         nr_pages);
   IGNORE_UNUSED_VARIABLE(app);

   FsFileInfo_getIOInfo(fileInfo, fhgfsInode, &ioInfo);

   err = read_cache_pages(mapping, pages, beegfs_readpages_add_page, context);
   beegfs_readpages_submit(context);

out:
   rpc_put(context);
   return err;
}



static int __beegfs_write_begin(struct file* filp, loff_t pos, unsigned len, struct page* page)
{
   struct append_range_descriptor* ard;
   int result = 0;

   if(filp->f_flags & O_APPEND)
   {
      if(pvr_present(page) )
         goto flush_page;

      ard = ard_from_page(page);
      if(!ard)
         goto page_clean;

      /* mapping_ard will not change here because the write path holds a reference to it. */
      if(ard == __mapping_ard(page->mapping) )
         goto success;
   }
   else
   {
      if(ard_from_page(page) )
         goto flush_page;

      if(!pvr_present(page) )
         goto success;

      if(pvr_can_merge(page, pos & ~PAGE_MASK, (pos & ~PAGE_MASK) + len - 1))
         goto success;
   }

flush_page:
   result = beegfs_flush_page(page);
   if(result)
      goto out_err;

page_clean:
   if(filp->f_flags & O_APPEND)
      ard_assign(page, __mapping_ard(page->mapping) );

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

   if(filp->f_flags & O_APPEND)
   {
      struct append_range_descriptor* ard = ard_from_page(page);

      BUG_ON(!ard);

      if( (filp->f_flags & O_APPEND) && ard->firstPageStart < 0)
      {
         mutex_lock(&ard->mtx);
         ard->firstPageIndex = page->index;
         ard->firstPageStart = pos & ~PAGE_MASK;
         mutex_unlock(&ard->mtx);
      }

      goto out;
   }

   /* write_iter updates isize for appends, since a later page may fail on an allocated ard */
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
   __set_page_dirty_nobuffers(page);

   unlock_page(page);
   put_page(page);

   return result;
}

#ifdef KERNEL_HAS_PREPARE_WRITE
static int beegfs_prepare_write(struct file* filp, struct page* page, unsigned from, unsigned to)
{
   return __beegfs_write_begin(filp, from, to - from + 1, page);
}

static int beegfs_commit_write(struct file* filp, struct page* page, unsigned from, unsigned to)
{
   return __beegfs_write_end(filp, from, to - from + 1, to - from + 1, page);
}
#else
static int beegfs_write_begin(struct file* filp, struct address_space* mapping, loff_t pos,
   unsigned len, unsigned flags, struct page** pagep, void** fsdata)
{
   pgoff_t index = pos >> PAGE_SHIFT;

   *pagep = grab_cache_page_write_begin(mapping, index, flags);
   if(!*pagep)
      return -ENOMEM;

   return __beegfs_write_begin(filp, pos, len, *pagep);
}

static int beegfs_write_end(struct file* filp, struct address_space* mapping, loff_t pos,
   unsigned len, unsigned copied, struct page* page, void* fsdata)
{
   return __beegfs_write_end(filp, pos, len, copied, page);
}
#endif

static int beegfs_releasepage(struct page* page, gfp_t gfp)
{
   IGNORE_UNUSED_VARIABLE(gfp);

   if(pvr_present(page) )
   {
      pvr_clear(page);
      return 1;
   }

   BUG_ON(!ard_from_page(page) );
   ard_assign(page, NULL);
   return 1;
}

static int beegfs_set_page_dirty(struct page* page)
{
   atomic_set(&BEEGFS_INODE(page->mapping->host)->modified, 1);

   if(ard_from_page(page) )
   {
      int err;

      err = beegfs_flush_page(page);
      if(err < 0)
         return err;
   }

   pvr_init(page);
   pvr_set_first(page, 0);
   pvr_set_last(page, PAGE_SIZE - 1);
   return __set_page_dirty_nobuffers(page);
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

   BUG_ON(!ard_from_page(page) );

   ard_assign(page, NULL);
}

#if !defined(KERNEL_HAS_INVALIDATEPAGE_RANGE)
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
      offset, iter->nr_segs);
   IGNORE_UNUSED_VARIABLE(app);

   result = FhgfsOpsRemoting_readfileVec(iter, offset, ioInfo, BEEGFS_INODE(inode));

   if(result < 0)
      return FhgfsOpsErr_toSysErr(-result);

   iov_iter_advance(iter, result);
   offset += result;

   if(iter->count > 0)
   {
      ssize_t readRes = __FhgfsOps_readSparse(filp, iter->iov->iov_base + offset + result, iter->count,
         offset + result);

      result += readRes;

      iov_iter_advance(iter, readRes);
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
      offset, iter->nr_segs);
   IGNORE_UNUSED_VARIABLE(app);
   IGNORE_UNUSED_VARIABLE(inode);

   result = FhgfsOpsRemoting_writefileVec(iter, offset, ioInfo, false);

   if(result < 0)
      return FhgfsOpsErr_toSysErr(-result);

   iov_iter_advance(iter, result);

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

   if(!iter_is_iovec(iter) )
   {
      printk(KERN_ERR "unexpected direct_IO rw %d\n", rw);
      WARN_ON(1);
      return -EINVAL;
   }

   switch(rw)
   {
      case READ:
         return beegfs_dIO_read(iocb, iter, offset, &ioInfo);

      case WRITE:
         return beegfs_dIO_write(iocb, iter, offset, &ioInfo);
   }

   BUG();
   return -EINVAL;
}

#if defined(KERNEL_HAS_IOV_DIO)
static ssize_t beegfs_direct_IO(struct kiocb* iocb, struct iov_iter* iter)
{
   return __beegfs_direct_IO(iov_iter_rw(iter), iocb, iter, iocb->ki_pos);
}
#elif defined(KERNEL_HAS_LONG_IOV_DIO)
static ssize_t beegfs_direct_IO(struct kiocb* iocb, struct iov_iter* iter, loff_t offset)
{
   return __beegfs_direct_IO(iov_iter_rw(iter), iocb, iter, offset);
}
#elif defined(KERNEL_HAS_DIRECT_IO_ITER)
static ssize_t beegfs_direct_IO(int rw, struct kiocb* iocb, struct iov_iter* iter, loff_t offset)
{
   return __beegfs_direct_IO(rw, iocb, iter, offset);
}
#else
static ssize_t beegfs_direct_IO(int rw, struct kiocb* iocb, const struct iovec* iov, loff_t pos,
      unsigned long nr_segs)
{
   struct iov_iter iter;

   BEEGFS_IOV_ITER_INIT(&iter, rw & RW_MASK, iov, nr_segs, iov_length(iov, nr_segs) );

   return __beegfs_direct_IO(rw, iocb, &iter, pos);
}
#endif

static int beegfs_launderpage(struct page* page)
{
   return beegfs_flush_page(page);
}

const struct address_space_operations fhgfs_addrspace_native_ops = {
   .readpage = beegfs_readpage,
   .writepage = beegfs_writepage,
   .releasepage = beegfs_releasepage,
   .set_page_dirty = beegfs_set_page_dirty,
   .invalidatepage = beegfs_invalidate_page,
   .direct_IO = beegfs_direct_IO,

   .readpages = beegfs_readpages,
   .writepages = beegfs_writepages,

   .launder_page = beegfs_launderpage,

#ifdef KERNEL_HAS_PREPARE_WRITE
   .prepare_write = beegfs_prepare_write,
   .commit_write = beegfs_commit_write,
#else
   .write_begin = beegfs_write_begin,
   .write_end = beegfs_write_end,
#endif
};
