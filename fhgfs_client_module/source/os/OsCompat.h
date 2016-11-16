/*
 * Compatibility functions for older Linux versions
 */

#ifndef OSCOMPAT_H_
#define OSCOMPAT_H_

#include <common/Common.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <asm/kmap_types.h>
#include <linux/compat.h>
#include <linux/list.h>
#include <linux/mount.h>
#include <linux/posix_acl_xattr.h>
#include <linux/swap.h>
#include <linux/writeback.h>

#ifdef KERNEL_HAS_TASK_IO_ACCOUNTING
   #include <linux/task_io_accounting_ops.h>
#endif

#ifdef KERNEL_HAS_GENERIC_SEMAPHORE
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
   extern void *memdup_user(const void __user *src, size_t len);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
   extern struct dentry *d_make_root(struct inode *root_inode);
#endif

#ifndef KERNEL_HAS_TASK_IO_ACCOUNTING
   static inline void task_io_account_read(size_t bytes);
   static inline void task_io_account_write(size_t bytes);
#endif

static inline int os_generic_permission(struct inode *inode, int mask);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
   extern int bdi_setup_and_register(struct backing_dev_info *bdi, char *name, unsigned int cap);
#endif

/**
 * generic_permission() compatibility function
 *
 * NOTE: Only kernels > 2.6.32 do have inode->i_op->check_acl, but as we do not
 *       support it anyway for now, we do not need a complete kernel version check for it.
 *       Also, in order to skip useless pointer references we just pass NULL here.
 */
int os_generic_permission(struct inode *inode, int mask)
{
   #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
      return generic_permission(inode, mask);
   #elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
      return generic_permission(inode, mask, 0, NULL);
   #else
      return generic_permission(inode, mask, NULL);
   #endif
}


#ifndef KERNEL_HAS_TASK_IO_ACCOUNTING

/**
 * No-op if kernel version does not support this.
 */
void task_io_account_read(size_t bytes)
{
}

/**
 * No-op if kernel version does not support this.
 */
void task_io_account_write(size_t bytes)
{
}

#endif // KERNEL_HAS_TASK_IO_ACCOUNTING


#ifndef KERNEL_HAS_D_MATERIALISE_UNIQUE
extern struct dentry* d_materialise_unique(struct dentry *dentry, struct inode *inode);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
/**
 * Taken from ext3 dir.c. is_compat_task() does work for all kernels, although it was already there.
 * So we are conservativ and only allow it for recent kernels.
 */
static inline int is_32bit_api(void)
{
#ifdef CONFIG_COMPAT
# ifdef in_compat_syscall
   return in_compat_syscall();
# else
   return is_compat_task();
# endif
#else
   return (BITS_PER_LONG == 32);
#endif
}
#else
static inline int is_32bit_api(void)
{
   return (BITS_PER_LONG == 32);
}
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)


#ifndef KERNEL_HAS_GET_UNALIGNED_LE
#include "AccessOk.h"
#endif

#ifndef KERNEL_HAS_I_UID_READ
static inline uid_t i_uid_read(const struct inode *inode)
{
   return inode->i_uid;
}

static inline gid_t i_gid_read(const struct inode *inode)
{
   return inode->i_gid;
}

static inline void i_uid_write(struct inode *inode, uid_t uid)
{
   inode->i_uid = uid;
}

static inline void i_gid_write(struct inode *inode, gid_t gid)
{
   inode->i_gid = gid;
}

#endif // KERNEL_HAS_I_UID_READ


#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23)
struct kmem_cache* OsCompat_initKmemCache(const char* cacheName, size_t cacheSize,
   void initFuncPtr(void* initObj, struct kmem_cache* cache, unsigned long flags) );
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
struct kmem_cache* OsCompat_initKmemCache(const char* cacheName, size_t cacheSize,
   void initFuncPtr(struct kmem_cache* cache, void* initObj) );
#else
struct kmem_cache* OsCompat_initKmemCache(const char* cacheName, size_t cacheSize,
   void initFuncPtr(void* initObj) );
#endif // LINUX_VERSION_CODE


// added to 3.13, backported to -stable
#ifndef list_next_entry
/**
 * list_next_entry - get the next element in list
 * @pos: the type * to cursor
 * @member: the name of the list_struct within the struct.
 */
#define list_next_entry(pos, member) \
   list_entry((pos)->member.next, typeof(*(pos)), member)
#endif


#ifndef KERNEL_HAS_LIST_IS_LAST
/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_last(const struct list_head *list,
            const struct list_head *head)
{
   return list->next == head;
}
#endif // KERNEL_HAS_LIST_IS_LAST

#ifndef list_first_entry
/**
 * list_first_entry - get the first element from a list
 * @ptr: the list head to take the element from.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
   list_entry((ptr)->next, type, member)
#endif // list_first_entry


static inline struct posix_acl* os_posix_acl_from_xattr(const void* value, size_t size)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
   return posix_acl_from_xattr(value, size);
#else
   return posix_acl_from_xattr(&init_user_ns, value, size);
#endif
}

static inline int os_posix_acl_to_xattr(const struct posix_acl* acl, void* buffer, size_t size)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
   return posix_acl_to_xattr(acl, buffer, size);
#else
   return posix_acl_to_xattr(&init_user_ns, acl, buffer, size);
#endif
}

#ifndef KERNEL_HAS_PAGE_ENDIO
static inline void page_endio(struct page *page, int rw, int err)
{
   if (rw == READ)
   {
      if (!err)
      {
         SetPageUptodate(page);
      }
      else
      {
         ClearPageUptodate(page);
         SetPageError(page);
      }

      unlock_page(page);
   }
   else
   { /* rw == WRITE */
      if (err)
      {
         SetPageError(page);
         if (page->mapping)
            mapping_set_error(page->mapping, err);
      }

      end_page_writeback(page);
   }
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
static inline int os_write_cache_pages(struct address_space* mapping, struct writeback_control* wbc,
   writepage_t writepage, void* data)
{
   return write_cache_pages(mapping, wbc, writepage, data);
}
#else
/* not adding a writepage_t typedef here because it already exists in older kernels */
extern int os_write_cache_pages(struct address_space* mapping, struct writeback_control* wbc,
   int (*writepage)(struct page*, struct writeback_control*, void*), void* data);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
# define BEEGFS_WORK_FUNCTION(name) void name(void* __workData)
# define BEEGFS_CURRENT_WORK_STRUCT ((struct work_struct*) __workData)
# define OS_INIT_WORK(work, fn) INIT_WORK((work), (fn), (work))
#else
# define BEEGFS_WORK_FUNCTION(name) void name(struct work_struct* __workData)
# define BEEGFS_CURRENT_WORK_STRUCT __workData
# define OS_INIT_WORK(work, fn) INIT_WORK((work), (fn))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0)
# define os_generic_write_checks generic_write_checks
#else
extern int os_generic_write_checks(struct file* filp, loff_t* offset, size_t* size, int isblk);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0)
#define rb_entry_safe(ptr, type, member) \
   ({ typeof(ptr) ____ptr = (ptr); \
      ____ptr ? rb_entry(____ptr, type, member) : NULL; \
   })

#define rbtree_postorder_for_each_entry_safe(pos, n, root, field) \
   for (pos = rb_entry_safe(rb_first_postorder(root), typeof(*pos), field); \
         pos && ({ n = rb_entry_safe(rb_next_postorder(&pos->field), \
         typeof(*pos), field); 1; }); \
      pos = n)

extern struct rb_node *rb_first_postorder(const struct rb_root *);
extern struct rb_node *rb_next_postorder(const struct rb_node *);
#endif

static inline int os_mnt_want_write(struct vfsmount *mnt)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
   if (mnt->mnt_sb->s_flags & MS_RDONLY)
      return -EROFS;
   else
      return 0;
#else
   return mnt_want_write(mnt);
#endif
}

static inline void os_mnt_drop_write(struct vfsmount *mnt)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
   /* noop before 2.6.26, as those didn't have mnt_want_write yet and so didn't take a
    * write ref-counter yet. */
   mnt_drop_write(mnt);
#endif
}

#ifndef KERNEL_HAS_CURRENT_UMASK
#define current_umask() (current->fs->umask)
#endif

#ifndef XATTR_NAME_POSIX_ACL_ACCESS
# define XATTR_POSIX_ACL_ACCESS  "posix_acl_access"
# define XATTR_NAME_POSIX_ACL_ACCESS XATTR_SYSTEM_PREFIX XATTR_POSIX_ACL_ACCESS
# define XATTR_POSIX_ACL_DEFAULT  "posix_acl_default"
# define XATTR_NAME_POSIX_ACL_DEFAULT XATTR_SYSTEM_PREFIX XATTR_POSIX_ACL_DEFAULT
#endif

#ifndef KERNEL_HAS_IS_VMALLOC_ADDR
static inline int is_vmalloc_addr(const void *x)
{
   unsigned long addr = (unsigned long) x;

   return addr >= VMALLOC_START && addr < VMALLOC_END;
}
#endif

#ifndef KERNEL_HAS_I_MMAP_LOCK
static inline void i_mmap_lock_read(struct address_space* mapping)
{
#if defined(KERNEL_HAS_I_MMAP_RWSEM)
   down_read(&mapping->i_mmap_rwsem);
#elif defined(KERNEL_HAS_I_MMAP_MUTEX)
   mutex_lock(&mapping->i_mmap_mutex);
#else
   spin_lock(&mapping->i_mmap_lock);
#endif
}

static inline void i_mmap_unlock_read(struct address_space* mapping)
{
#if defined(KERNEL_HAS_I_MMAP_RWSEM)
   up_read(&mapping->i_mmap_rwsem);
#elif defined(KERNEL_HAS_I_MMAP_MUTEX)
   mutex_unlock(&mapping->i_mmap_mutex);
#else
   spin_unlock(&mapping->i_mmap_lock);
#endif
}
#endif

static inline bool beegfs_hasMappings(struct inode* inode)
{
#ifdef KERNEL_HAS_I_MMAP_RBTREE
   if (!RB_EMPTY_ROOT(&inode->i_mapping->i_mmap))
      return true;
#else
   if (!prio_tree_empty(&inode->i_mapping->i_mmap))
      return true;
#endif

#ifdef KERNEL_HAS_I_MMAP_NONLINEAR
   if (!list_empty(&inode->i_mapping->i_mmap_nonlinear))
      return true;
#endif

   return false;
}

#ifndef KERNEL_HAS_INODE_LOCK
static inline void os_inode_lock(struct inode* inode)
{
   mutex_lock(&inode->i_mutex);
}

static inline void os_inode_unlock(struct inode* inode)
{
   mutex_unlock(&inode->i_mutex);
}
#else
static inline void os_inode_lock(struct inode* inode)
{
   inode_lock(inode);
}

static inline void os_inode_unlock(struct inode* inode)
{
   inode_unlock(inode);
}
#endif

#endif /* OSCOMPAT_H_ */
