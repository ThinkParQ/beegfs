#ifndef FHGFSOPS_VERSIONS_H_
#define FHGFSOPS_VERSIONS_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/pagevec.h>
#include <linux/pagemap.h>
#include <linux/page-flags.h>

#ifndef KERNEL_HAS_F_DENTRY
#define f_dentry        f_path.dentry
#endif // KERNEL_HAS_F_DENTRY

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
   int FhgfsOps_permission(struct inode *inode, int mask);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
   int FhgfsOps_permission(struct inode *inode, int mask, unsigned int flags);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
   int FhgfsOps_permission(struct inode *inode, int mask);
#else
   /* <= 2.6.26 */
   int FhgfsOps_permission(struct inode *inode, int mask, struct nameidata *nd);
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,38)
extern int FhgfsOps_getSB(struct file_system_type *fs_type,
   int flags, const char *dev_name, void *data, struct vfsmount *mnt);
#else
extern struct dentry* FhgfsOps_mount(struct file_system_type *fs_type,
   int flags, const char *dev_name, void *data);
#endif // LINUX_VERSION_CODE


extern int FhgfsOps_statfs(struct dentry* dentry, struct kstatfs* kstatfs);


extern int FhgfsOps_flush(struct file *file, fl_owner_t id);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23)
extern void FhgfsOps_initInodeOnce(void* inode, struct kmem_cache* cache, unsigned long flags);
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
extern void FhgfsOps_initInodeOnce(struct kmem_cache* cache, void* inode);
#else
extern void FhgfsOps_initInodeOnce(void* inode);
#endif // LINUX_VERSION_CODE


////////////// start of kernel method emulators //////////////


#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
extern loff_t generic_file_llseek_unlocked(struct file *file, loff_t offset, int origin);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
extern loff_t generic_file_llseek_unlocked(struct file *file, loff_t offset, int origin);
#endif // LINUX_VERSION_CODE

#ifndef KERNEL_HAS_DROP_NLINK
extern void drop_nlink(struct inode *inode);
#endif // KERNEL_HAS_DROP_NLINK

#ifndef KERNEL_HAS_CLEAR_NLINK
extern void clear_nlink(struct inode *inode);
#endif // KERNEL_HAS_CLEAR_NLINK

#ifndef KERNEL_HAS_INC_NLINK
extern void inc_nlink(struct inode *inode);
#endif // KERNEL_HAS_INC_NLINK

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
static inline void set_nlink(struct inode *inode, unsigned int nlink);
#endif // LINUX_VERSION_CODE

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,27)
static inline void pagevec_lru_add_file(struct pagevec *pvec);
static inline void __pagevec_lru_add_file(struct pagevec *pvec);
#endif // LINUX_VERSION_CODE

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)
static inline int trylock_page(struct page *page);
#endif // LINUX_VERSION_CODE

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
static inline void mapping_set_error(struct address_space *mapping, int error);
#endif // LINUX_VERSION_CODE

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
char *dentry_path_raw(struct dentry *dentry, char *buf, int buflen);
#endif

#ifndef KERNEL_HAS_FILE_INODE
static inline struct inode *file_inode(struct file *f);
#endif // KERNEL_HAS_FILE_INODE



#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
/**
 * Note: This is just an emulator that does the job for old kernels.
 */
void set_nlink(struct inode *inode, unsigned int nlink)
{
   inode->i_nlink = nlink;
}
#endif // LINUX_VERSION_CODE


#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,27)

/**
 * Note: This is just a wrapper that calls the old function name for old kernels.
 */
void pagevec_lru_add_file(struct pagevec *pvec)
{
   pagevec_lru_add(pvec);
}

/**
 * Note: This is just a wrapper that calls the old function name for old kernels.
 */
void __pagevec_lru_add_file(struct pagevec *pvec)
{
   __pagevec_lru_add(pvec);
}

#endif // LINUX_VERSION_CODE


#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,26)

/**
 * Note: This is just an emulator that does the job for old kernels.
 */
int trylock_page(struct page *page)
{
   return (likely(!test_and_set_bit(PG_locked, &page->flags) ) );
}

#endif // LINUX_VERSION_CODE

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)

/**
 * Note: This is just an emulator that does the job for old kernels.
 */
void mapping_set_error(struct address_space *mapping, int error)
{
   if(unlikely(error) )
   {
      if(error == -ENOSPC)
         set_bit(AS_ENOSPC, &mapping->flags);
      else
         set_bit(AS_EIO, &mapping->flags);
   }
}

#endif // LINUX_VERSION_CODE

#ifndef KERNEL_HAS_IHOLD
/*
 * get additional reference to inode; caller must already hold one.
 */
static inline void ihold(struct inode *inode)
{
   WARN_ON(atomic_inc_return(&inode->i_count) < 2);
}
#endif // KERNEL_HAS_IHOLD

#ifndef KERNEL_HAS_FILE_INODE

struct inode *file_inode(struct file *f)
{
   return f->f_dentry->d_inode;
}

#endif // KERNEL_HAS_FILE_INODE



#endif /*FHGFSOPS_VERSIONS_H_*/
