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

#ifndef os_iov_iter_h_gkoNxI6c8tqzi7GeSKSVlb
#define os_iov_iter_h_gkoNxI6c8tqzi7GeSKSVlb

#include <linux/kernel.h>
#include <linux/uio.h>
#include <linux/version.h>
#include <linux/uaccess.h>

#if defined(KERNEL_HAS_IOV_ITER_IN_FS)
#include <linux/fs.h>
#else
#include <linux/uio.h>
#endif


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


#ifdef KERNEL_HAS_ITER_KVEC
#define beegfs_iov_for_each(iov, iter, start)       \
   if ((iov_iter_type(start) == ITER_IOVEC) ||      \
       (iov_iter_type(start) == ITER_KVEC))         \
   for (iter = (*start);                            \
        (iter).count &&                             \
        ((iov = iov_iter_iovec(&(iter))), 1);       \
          iov_iter_advance(&(iter), (iov).iov_len))
#else
#define beegfs_iov_for_each(iov, iter, start)       \
   for (iter = (*start);                            \
        (iter).count &&                             \
        ((iov = iov_iter_iovec(&(iter))), 1);       \
          iov_iter_advance(&(iter), (iov).iov_len))
#endif

#ifndef KERNEL_HAS_IOV_ITER_TRUNCATE
static inline void iov_iter_truncate(struct iov_iter *i, size_t count)
{
   if (i->count > count)
      i->count = count;
}
#endif


#ifndef KERNEL_HAS_COPY_FROM_ITER
static inline size_t copy_from_iter(void* to, size_t bytes, struct iov_iter* i)
{
   /* FIXME: check for != IOV iters */

   size_t copy, left, wanted;
   struct iovec iov;
   char __user* buf;

   if (unlikely(bytes > i->count) )
      bytes = i->count;

   if (unlikely(!bytes) )
      return 0;

   wanted = bytes;
   iov = iov_iter_iovec(i);
   buf = iov.iov_base;
   copy = min(bytes, iov.iov_len);

   left = __copy_from_user(to, buf, copy);
   copy -= left;
   to += copy;
   bytes -= copy;
   while (unlikely(!left && bytes) ) {
      iov_iter_advance(i, copy);
      iov = iov_iter_iovec(i);
      buf = iov.iov_base;
      copy = min(bytes, iov.iov_len);
      left = __copy_from_user(to, buf, copy);
      copy -= left;
      to += copy;
      bytes -= copy;
   }

   iov_iter_advance(i, copy);
   return wanted - bytes;
}

static inline size_t copy_to_iter(const void* from, size_t bytes, struct iov_iter* i)
{
   /* FIXME: check for != IOV iters */

   size_t copy, left, wanted;
   struct iovec iov;
   char __user *buf;

   if (unlikely(bytes > i->count) )
      bytes = i->count;

   if (unlikely(!bytes) )
      return 0;

   wanted = bytes;
   iov = iov_iter_iovec(i);
   buf = iov.iov_base;
   copy = min(bytes, iov.iov_len);

   left = __copy_to_user(buf, from, copy);
   copy -= left;
   from += copy;
   bytes -= copy;
   while (unlikely(!left && bytes) ) {
      iov_iter_advance(i, copy);
      iov = iov_iter_iovec(i);
      copy = min(bytes, iov.iov_len);
      left = __copy_to_user(buf, from, copy);
      copy -= left;
      from += copy;
      bytes -= copy;
   }

   iov_iter_advance(i, copy);
   return wanted - bytes;
}


//XXX this code is written to work for ITER_IOVEC but will also work (due to same layout)
// for ITER_KVEC. The type punning is probably illegal as far as C is concerned, but that's
// how it is done even in the Linux kernel.
#define iterate_iovec(i, n, __v, __p, skip, STEP) {	\
	size_t left;					\
	size_t wanted = n;				\
	__p = i->iov;					\
	__v.iov_len = min(n, __p->iov_len - skip);	\
	if (likely(__v.iov_len)) {			\
		__v.iov_base = __p->iov_base + skip;	\
		left = (STEP);				\
		__v.iov_len -= left;			\
		skip += __v.iov_len;			\
		n -= __v.iov_len;			\
	} else {					\
		left = 0;				\
	}						\
	while (unlikely(!left && n)) {			\
		__p++;					\
		__v.iov_len = min(n, __p->iov_len);	\
		if (unlikely(!__v.iov_len))		\
			continue;			\
		__v.iov_base = __p->iov_base;		\
		left = (STEP);				\
		__v.iov_len -= left;			\
		skip = __v.iov_len;			\
		n -= __v.iov_len;			\
	}						\
	n = wanted - n;					\
}

//XXX this code is written to work for ITER_IOVEC -- see comment above.
#define iterate_and_advance(i, n, v, I, B, K) {			\
	size_t skip = i->iov_offset;				\
   const struct iovec *iov;			\
   struct iovec v;					\
   iterate_iovec(i, n, v, iov, skip, (I))		\
   if (skip == iov->iov_len) {			\
      iov++;					\
      skip = 0;				\
   }						\
   i->nr_segs -= iov - i->iov;			\
   i->iov = iov;					\
	i->count -= n;						\
	i->iov_offset = skip;					\
}

static inline size_t iov_iter_zero(size_t bytes, struct iov_iter *i)
{
	iterate_and_advance(i, bytes, v,
		clear_user(v.iov_base, v.iov_len),
		memzero_page(v.bv_page, v.bv_offset, v.bv_len),
		memset(v.iov_base, 0, v.iov_len)
	)

	return bytes;
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
   return iov_iter_type(&iter->_iov_iter) & ITER_PIPE;
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

#endif
