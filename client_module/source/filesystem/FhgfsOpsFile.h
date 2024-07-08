#ifndef FHGFSOPSFILE_H_
#define FHGFSOPSFILE_H_

#include <app/App.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/Time.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <filesystem/FsDirInfo.h>
#include <filesystem/FsFileInfo.h>
#include <toolkit/NoAllocBufferStore.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include "FhgfsOps_versions.h"
#include "FhgfsOpsInode.h"


#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/pagemap.h>
#include <linux/uio.h>


#ifndef SEEK_SET
#define SEEK_SET  0  /* seek relative to beginning of file */
#define SEEK_CUR  1  /* seek relative to current file position */
#define SEEK_END  2  /* seek relative to end of file */
#endif


// forward declaration
struct App;


extern struct file_operations fhgfs_file_buffered_ops;
extern struct file_operations fhgfs_file_pagecache_ops;

extern struct file_operations fhgfs_dir_ops;

extern struct address_space_operations fhgfs_address_ops;
extern struct address_space_operations fhgfs_address_pagecache_ops;


extern loff_t FhgfsOps_llseekdir(struct file *file, loff_t offset, int origin);
extern loff_t FhgfsOps_llseek(struct file *file, loff_t offset, int origin);

extern int FhgfsOps_opendirIncremental(struct inode* inode, struct file* file);
extern int FhgfsOps_releasedir(struct inode* inode, struct file* file);

#ifdef KERNEL_HAS_ITERATE_DIR
extern int FhgfsOps_iterateIncremental(struct file* file, struct dir_context* ctx);
#else
extern int FhgfsOps_readdirIncremental(struct file* file, void* buf, filldir_t filldir);
#endif // LINUX_VERSION_CODE

extern int FhgfsOps_open(struct inode* inode, struct file* file);
extern int FhgfsOps_openReferenceHandle(App* app, struct inode* inode, struct file* file,
   unsigned openFlags, LookupIntentInfoOut* lookupInfo, uint64_t* outVersion);
extern int FhgfsOps_release(struct inode* inode, struct file* file);


#ifdef KERNEL_HAS_FSYNC_RANGE /* added in vanilla 3.1 */
   int FhgfsOps_fsync(struct file* file, loff_t start, loff_t end, int datasync);
#elif !defined(KERNEL_HAS_FSYNC_DENTRY)
   int FhgfsOps_fsync(struct file* file, int datasync);
#else
   /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,34) */
   int FhgfsOps_fsync(struct file* file, struct dentry* dentry, int datasync);
#endif // LINUX_VERSION_CODE

extern int __FhgfsOps_flush(App* app, struct file *file, bool discardCacheOnError,
   bool forceRemoteFlush, bool checkSession, bool isClose);

extern int FhgfsOps_flock(struct file* file, int cmd, struct file_lock* fileLock);
extern int FhgfsOps_lock(struct file* file, int cmd, struct file_lock* fileLock);

extern int FhgfsOps_readlink(struct dentry* dentry, char __user* buf, int size);

extern ssize_t FhgfsOps_read(struct file* file, char __user *buf, size_t size,
   loff_t* offsetPointer);
extern ssize_t FhgfsOps_write(struct file* file, const char __user *buf, size_t size,
   loff_t* offsetPointer);

ssize_t FhgfsOps_read_iter(struct kiocb *iocb, struct iov_iter *to);
ssize_t FhgfsOps_write_iter(struct kiocb *iocb, struct iov_iter *from);

extern int FhgfsOps_mmap(struct file *, struct vm_area_struct *);

extern ssize_t FhgfsOps_directIO(struct kiocb *iocb, struct iov_iter *iter);

extern int FhgfsOps_releaseCancelLocks(struct inode* inode, struct file* file);
extern ssize_t __FhgfsOps_readSparse(struct file* file, struct iov_iter *iter, size_t size,
   loff_t offset);


// getters & setters
static inline FsObjectInfo* __FhgfsOps_getObjectInfo(struct file* file);
static inline FsDirInfo* __FhgfsOps_getDirInfo(struct file* file);
static inline void __FhgfsOps_setDirInfo(FsDirInfo* dirInfo, struct file* outFile);
static inline FsFileInfo* __FhgfsOps_getFileInfo(struct file* file);
static inline void __FhgfsOps_setFileInfo(FsFileInfo* fileInfo, struct file* outFile);
static inline int __FhgfsOps_getCurrentLockPID(void);
static inline int64_t __FhgfsOps_getCurrentLockFD(struct file* file);


FsObjectInfo* __FhgfsOps_getObjectInfo(struct file* file)
{
   return (FsObjectInfo*)file->private_data;
}

FsDirInfo* __FhgfsOps_getDirInfo(struct file* file)
{
   return (FsDirInfo*)file->private_data;
}

void __FhgfsOps_setDirInfo(FsDirInfo* dirInfo, struct file* outFile)
{
   outFile->private_data = dirInfo;
}

FsFileInfo* __FhgfsOps_getFileInfo(struct file* file)
{
   return (FsFileInfo*)file->private_data;
}

void __FhgfsOps_setFileInfo(FsFileInfo* fileInfo, struct file* outFile)
{
   outFile->private_data = fileInfo;
}

/**
 * @return lock pid of the current process (which is _not_ current->pid)
 */
int __FhgfsOps_getCurrentLockPID(void)
{
   /* note: tgid (not current->pid) is the actual equivalent of the number that getpid() returns to
      user-space and is also the thing that is assigned to fl_pid in "<kernel>/fs/locks.c" */

   return current->tgid;
}

/**
 * @return virtual file descriptor for entry locking (which is _not_ the user-space fd)
 */
int64_t __FhgfsOps_getCurrentLockFD(struct file* file)
{
   /* note: we can't get the user-space fd here and the rest of the kernel entry locking routines
      in "<kernel>/fs/locks.c" also uses the struct file pointer for comparison instead, so we
      return that one here. */

   return (size_t)file;
}

#endif /*FHGFSOPSFILE_H_*/
