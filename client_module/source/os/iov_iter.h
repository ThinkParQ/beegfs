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

/*
 * In kernels 3.15 to 6.3 there was iov_iter_iovec(), returning the first iovec
 * in an iov_iter of type ITER_IOVEC.
 * 6.4 removes and started using macro iter_iov_addr & iter_iov_len.
 * Using those now and providing a shim for older kernels.
 */
#if !defined(KERNEL_HAS_ITER_IOV_ADDR)
#define iter_iov_addr(iter)     (iter_iov(iter)->iov_base + (iter)->iov_offset)
#define iter_iov_len(iter)      (iter_iov(iter)->iov_len - (iter)->iov_offset)
#endif

#ifndef KERNEL_HAS_IOV_ITER_INIT_DIR
#error We require kernels that have a "direction" parameter to iov_iter_init().
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


static inline int beegfs_iov_iter_is_iovec(const struct iov_iter *iter)
{
   return iov_iter_type(iter) == ITER_IOVEC;
}

// TODO: Now that ITER_KVEC is required across all kernels, is this function still needed?
static inline struct iov_iter *beegfs_get_iovec_iov_iter(struct iov_iter *iter)
{
   BUG_ON(!beegfs_iov_iter_is_iovec(iter));
   return iter;
}

static inline unsigned long beegfs_iov_iter_nr_segs(const struct iov_iter *iter)
{
   return iter->nr_segs;
}

static inline void beegfs_iov_iter_clear(struct iov_iter *iter)
{
   iter->count = 0;
}

#ifdef KERNEL_HAS_ITER_PIPE
static inline bool beegfs_is_pipe_iter(struct iov_iter * iter)
{
   return iov_iter_type(iter) == ITER_PIPE;
}
#endif

#define BEEGFS_IOV_ITER_INIT iov_iter_init

static inline void BEEGFS_IOV_ITER_KVEC(struct iov_iter *iter, int direction,
   const struct kvec* kvec, unsigned long nr_segs, size_t count)
{
#ifndef KERNEL_HAS_IOV_ITER_KVEC_NO_TYPE_FLAG_IN_DIRECTION
   direction |= ITER_KVEC;
#endif
   iov_iter_kvec(iter, direction, kvec, nr_segs, count);
}

static inline void BEEGFS_IOV_ITER_BVEC(struct iov_iter *iter, int direction,
   const struct bio_vec* bvec, unsigned long nr_segs, size_t count)
{
#ifndef KERNEL_HAS_IOV_ITER_KVEC_NO_TYPE_FLAG_IN_DIRECTION
   direction |= ITER_BVEC;
#endif
   iov_iter_bvec(iter, direction, bvec, nr_segs, count);
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
      iov_iter_count() on the sanitized_iter field.

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
   struct iov_iter sanitized_iter;
};

void beegfs_readsink_reserve(BeeGFS_ReadSink *rs, struct iov_iter *iter, size_t size);
void beegfs_readsink_release(BeeGFS_ReadSink *rs);




/*
 We have lots of code locations where we need to read or write memory using a
 pointer + length pair, but need to use an iov_iter based API. This always
 leads to boilerplate where struct iovec and struct iov_iter values have to be
 declared on the stack. The following hack is meant to reduce that boilerplate.
 */
#define STACK_ALLOC_BEEGFS_ITER_IOV(ptr, size, direction) \
   ___BEEGFS_IOV_ITER_INIT(&(struct iov_iter){0}, &(struct iovec){0}, (ptr), (size), (direction))

#define STACK_ALLOC_BEEGFS_ITER_KVEC(ptr, size, direction) \
   ___BEEGFS_IOV_ITER_KVEC(&(struct iov_iter){0}, &(struct kvec){0}, (ptr), (size), (direction))

static inline struct iov_iter *___BEEGFS_IOV_ITER_INIT(
      struct iov_iter *iter, struct iovec *iovec,
      const char __user *ptr, size_t size, int direction)
{
   unsigned nr_segs = 1;
   *iovec = (struct iovec) {
      .iov_base = (char __user *) ptr,
      .iov_len = size,
   };
   BEEGFS_IOV_ITER_INIT(iter, direction, iovec, nr_segs, size);
   return iter;
}

static inline struct iov_iter *___BEEGFS_IOV_ITER_KVEC(
      struct iov_iter *iter, struct kvec* kvec,
      const char *ptr, size_t size, int direction)
{
   unsigned nr_segs = 1;
   *kvec = (struct kvec) {
      .iov_base = (char *) ptr,
      .iov_len = size,
   };
   BEEGFS_IOV_ITER_KVEC(iter, direction, kvec, nr_segs, size);
   return iter;
}
