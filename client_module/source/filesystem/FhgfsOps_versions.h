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

#if defined(KERNEL_HAS_IDMAPPED_MOUNTS)
   int FhgfsOps_permission(struct mnt_idmap* idmap, struct inode *inode, int mask);
#elif defined(KERNEL_HAS_USER_NS_MOUNTS)
   int FhgfsOps_permission(struct user_namespace* mnt_userns, struct inode *inode, int mask);
#elif defined(KERNEL_HAS_PERMISSION_2)
   int FhgfsOps_permission(struct inode *inode, int mask);
#elif defined(KERNEL_HAS_PERMISSION_FLAGS)
   int FhgfsOps_permission(struct inode *inode, int mask, unsigned int flags);
#else
   /* <= 2.6.26 */
   int FhgfsOps_permission(struct inode *inode, int mask, struct nameidata *nd);
#endif

#ifdef KERNEL_HAS_GET_SB_NODEV
extern int FhgfsOps_getSB(struct file_system_type *fs_type,
   int flags, const char *dev_name, void *data, struct vfsmount *mnt);
#else
extern struct dentry* FhgfsOps_mount(struct file_system_type *fs_type,
   int flags, const char *dev_name, void *data);
#endif // LINUX_VERSION_CODE


extern int FhgfsOps_statfs(struct dentry* dentry, struct kstatfs* kstatfs);


extern int FhgfsOps_flush(struct file *file, fl_owner_t id);

#if defined(KERNEL_HAS_KMEMCACHE_CACHE_FLAGS_CTOR)
extern void FhgfsOps_initInodeOnce(void* inode, struct kmem_cache* cache, unsigned long flags);
#elif defined(KERNEL_HAS_KMEMCACHE_CACHE_CTOR)
extern void FhgfsOps_initInodeOnce(struct kmem_cache* cache, void* inode);
#else
extern void FhgfsOps_initInodeOnce(void* inode);
#endif // LINUX_VERSION_CODE


////////////// start of kernel method emulators //////////////


#ifndef KERNEL_HAS_GENERIC_FILE_LLSEEK_UNLOCKED
extern loff_t generic_file_llseek_unlocked(struct file *file, loff_t offset, int origin);
#endif // LINUX_VERSION_CODE

#ifndef KERNEL_HAS_SET_NLINK
static inline void set_nlink(struct inode *inode, unsigned int nlink);
#endif // LINUX_VERSION_CODE

#ifndef KERNEL_HAS_DENTRY_PATH_RAW
char *dentry_path_raw(struct dentry *dentry, char *buf, int buflen);
#endif

#ifndef KERNEL_HAS_FILE_INODE
static inline struct inode *file_inode(struct file *f);
#endif // KERNEL_HAS_FILE_INODE



#ifndef KERNEL_HAS_SET_NLINK
/**
 * Note: This is just an emulator that does the job for old kernels.
 */
void set_nlink(struct inode *inode, unsigned int nlink)
{
   inode->i_nlink = nlink;
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

#ifndef KERNEL_HAS_FILE_DENTRY
static inline struct dentry *file_dentry(const struct file *file)
{
   return file->f_path.dentry;
}
#endif

/* some ofeds backport this too - and put a define around it ... */
#if !defined(KERNEL_HAS_FILE_INODE) && !defined(file_inode)

struct inode *file_inode(struct file *f)
{
   return file_dentry(f)->d_inode;
}

#endif // KERNEL_HAS_FILE_INODE



#endif /*FHGFSOPS_VERSIONS_H_*/
