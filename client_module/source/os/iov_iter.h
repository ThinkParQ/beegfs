/*
 * compatibility for older kernels. this code is mostly taken from include/linux/uio.h,
 * include/linuxfs/fs.h and associated .c files.
 *
 * the originals are licensed as:
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#pragma once

#include <linux/kernel.h>
#include <linux/uio.h>
#include <linux/version.h>
#include <linux/uaccess.h>

#include <linux/bvec.h>

#ifndef KERNEL_HAS_ITER_KVEC
#error ITER_KVEC is a required feature
#endif

#ifndef KERNEL_HAS_ITER_IS_IOVEC
#error iter_is_iovec() is a required feature
#endif

#ifndef KERNEL_HAS_IOV_ITER_IOVEC
#error iov_iter_iovec() is a required feature
#endif



#ifndef KERNEL_HAS_IOV_ITER_TYPE
static inline int iov_iter_type(const struct iov_iter *i)
{
   return i->type & ~(READ | WRITE);
}
#endif


#ifndef KERNEL_HAS_IOV_ITER_IS_PIPE
static inline bool iov_iter_is_pipe(struct iov_iter* iter)
{
#ifdef KERNEL_HAS_ITER_PIPE
   return iov_iter_type(iter) == ITER_PIPE;
#else
   return false;
#endif
}
#endif


// TODO: we can consider removing this wrapper. By now, only the struct
// iov_iter member is left.
typedef struct
{
   struct iov_iter _iov_iter;
} BeeGFS_IovIter;

static inline int beegfs_iov_iter_is_iovec(const BeeGFS_IovIter *iter)
{
   return iov_iter_type(&iter->_iov_iter) == ITER_IOVEC;
}

// by now, we can assume that we have ITER_IOVEC and ITER_KVEC available
// TODO: we can get rid of more compat code because of this assumption
static inline int beegfs_iov_iter_type(BeeGFS_IovIter *iter)
{
   return iov_iter_type(&iter->_iov_iter);
}

// TODO: Now that ITER_KVEC is required across all kernels, is this function still needed?
static inline struct iov_iter *beegfs_get_iovec_iov_iter(BeeGFS_IovIter *iter)
{
   BUG_ON(!beegfs_iov_iter_is_iovec(iter));
   return &iter->_iov_iter;
}

static inline struct iov_iter iov_iter_from_beegfs_iov_iter(BeeGFS_IovIter i)
{
   return i._iov_iter;
}

static inline BeeGFS_IovIter beegfs_iov_iter_from_iov_iter(struct iov_iter i)
{

   BeeGFS_IovIter result = { ._iov_iter = i };
   return result;
}

static inline size_t beegfs_iov_iter_count(const BeeGFS_IovIter *iter)
{
   // Not all versions of iov_iter_count() take a const pointer...
   return iov_iter_count(&((BeeGFS_IovIter *) iter)->_iov_iter);
}

static inline unsigned long beegfs_iov_iter_nr_segs(const BeeGFS_IovIter *iter)
{
   return iter->_iov_iter.nr_segs;
}

static inline void beegfs_iov_iter_clear(BeeGFS_IovIter *iter)
{
   iter->_iov_iter.count = 0;
}

static inline size_t beegfs_copy_to_iter(const void *addr, size_t bytes, BeeGFS_IovIter *i)
{
   return copy_to_iter(addr, bytes, &i->_iov_iter);
}

static inline size_t beegfs_copy_from_iter(void *addr, size_t bytes, BeeGFS_IovIter *i)
{
   return copy_from_iter(addr, bytes, &i->_iov_iter);
}

static inline size_t beegfs_iov_iter_zero(size_t bytes, BeeGFS_IovIter *iter)
{
   return iov_iter_zero(bytes, &iter->_iov_iter);
}

static inline void beegfs_iov_iter_truncate(BeeGFS_IovIter *iter, u64 count)
{
   iov_iter_truncate(&iter->_iov_iter, count);
}

static inline void beegfs_iov_iter_advance(BeeGFS_IovIter *iter, size_t bytes)
{
   iov_iter_advance(&iter->_iov_iter, bytes);
}

static inline bool beegfs_is_pipe_iter(BeeGFS_IovIter * iter)
{
#ifdef KERNEL_HAS_ITER_PIPE
   return iov_iter_type(&iter->_iov_iter) == ITER_PIPE;
#else
   return false;
#endif
}

static inline void BEEGFS_IOV_ITER_INIT(BeeGFS_IovIter *iter, int direction,
   const struct iovec* iov, unsigned long nr_segs, size_t count)
{

#ifndef KERNEL_HAS_IOV_ITER_INIT_DIR
#error We require kernels that have a "direction" parameter to iov_iter_init().
#endif

   iov_iter_init(&iter->_iov_iter, direction, iov, nr_segs, count);
}

static inline void BEEGFS_IOV_ITER_KVEC(BeeGFS_IovIter *iter, int direction,
   const struct kvec* kvec, unsigned long nr_segs, size_t count)
{
#ifndef KERNEL_HAS_IOV_ITER_INIT_DIR
#error We require kernels that have a "direction" parameter to iov_iter_init().
#endif

#ifndef KERNEL_HAS_IOV_ITER_KVEC_NO_TYPE_FLAG_IN_DIRECTION
   direction |= ITER_KVEC;
#endif

   iov_iter_kvec(&iter->_iov_iter, direction, kvec, nr_segs, count);
}

static inline void BEEGFS_IOV_ITER_BVEC(BeeGFS_IovIter *iter, int direction,
   const struct bio_vec* bvec, unsigned long nr_segs, size_t count)
{
#ifndef KERNEL_HAS_IOV_ITER_INIT_DIR
#error We require kernels that have a "direction" parameter to iov_iter_init().
#endif

#ifndef KERNEL_HAS_IOV_ITER_KVEC_NO_TYPE_FLAG_IN_DIRECTION
   direction |= ITER_BVEC;
#endif

   iov_iter_bvec(&iter->_iov_iter, direction, bvec, nr_segs, count);
}



/*
   BeeGFS_ReadSink

   We can't get parallel reads to work easily with ITER_PIPE. That type of iter
   doesn't allow splitting up a region easily for parallel writing. The reason
   is that the iov_iter_advance() implementation for ITER_PIPE modifies shared
   state (the pipe_inode structure).

   The BeeGFS_ReadSink structure allows to abstract from that concern by
   converting to an ITER_BVEC iter where necessary.

   Use is as follows:
      1) Initialize the struct by zeroing out, or using a {0} initializer.
      This allows the cleanup routine to work even if nothing was ever
      allocated.

      1) Call _reserve() to set up a view of the given size into a given iov_iter
      struct. If the given iov_iter is not of type ITER_PIPE, it will be copied
      straight to the "sanitized_iter" field.  Otherwise (if it is an
      ITER_PIPE), an ITER_BVEC iterator will be made by allocating pages from
      the pipe and setting up a bio_vec for each page.

      Note that this can fail in low memory situations. The size of the view
      that was successfully allocated can be queried by calling
      beegfs_iov_iter_count() on the sanitized_iter field.

      2) The sanitized_iter field should be used to read data. The field can be
      used destructively. In particular it is safe to call iov_iter_advance()
      on it in order to partition the view for multiple parallel reads.

      3) When reads are done, probably, iov_iter_advance() should be called on
      the iter that was given to _reserve().

      4) Call _release() to give back the pages that were reserved in step 2).
      If the struct was properly initialized in step 1), is safe to call
      _release() even if _reserve() was never called. This is useful when cleaning
      up state after an early exit.

      5) Go back to 2) if necessary, to copy more data.
*/

typedef struct _BeeGFS_ReadSink BeeGFS_ReadSink;
struct _BeeGFS_ReadSink {
   size_t npages;  // Number of pages currently in use (get_page())
   struct page **pages;  // 0..npages
   struct bio_vec *bvecs;  // 0..npages

   // output value
   BeeGFS_IovIter sanitized_iter;
};

void beegfs_readsink_reserve(BeeGFS_ReadSink *rs, struct iov_iter *iter, size_t size);
void beegfs_readsink_release(BeeGFS_ReadSink *rs);
