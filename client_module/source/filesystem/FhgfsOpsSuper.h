#ifndef FHGFSOPSSUPER_H_
#define FHGFSOPSSUPER_H_

#include <app/App.h>
#include <common/Common.h>
#include "FhgfsOps_versions.h"

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/backing-dev.h>
#include <linux/seq_file.h>


#define BEEGFS_MAGIC                    0x19830326 /* some random number to identify fhgfs */

#define BEEGFS_STATFS_BLOCKSIZE_SHIFT   19 /* bit shift to compute reported block size in statfs() */
#define BEEGFS_STATFS_BLOCKSIZE         (1UL << BEEGFS_STATFS_BLOCKSIZE_SHIFT)


struct FhgfsSuperBlockInfo;
typedef struct FhgfsSuperBlockInfo FhgfsSuperBlockInfo;



extern int FhgfsOps_registerFilesystem(void);
extern int FhgfsOps_unregisterFilesystem(void);

extern void FhgfsOps_putSuper(struct super_block* sb);
extern void FhgfsOps_killSB(struct super_block* sb);
#ifdef KERNEL_HAS_SHOW_OPTIONS_DENTRY
extern int FhgfsOps_showOptions(struct seq_file* sf, struct dentry* dentry);
#else
extern int FhgfsOps_showOptions(struct seq_file* sf, struct vfsmount* vfs);
#endif

extern int  FhgfsOps_fillSuper(struct super_block* sb, void* rawMountOptions, int silent);

// getters & setters
static inline App* FhgfsOps_getApp(struct super_block* sb);
static inline struct backing_dev_info* FhgfsOps_getBdi(struct super_block* sb);

static inline bool FhgfsOps_getHasRootEntryInfo(struct super_block* sb);
static inline void FhgfsOps_setHasRootEntryInfo(struct super_block* sb, bool isInited);
static inline bool FhgfsOps_getIsRootInited(struct super_block* sb);
static inline void FhgfsOps_setIsRootInited(struct super_block* sb, bool isInited);


struct FhgfsSuperBlockInfo
{
   App app;
#if !defined(KERNEL_HAS_SUPER_SETUP_BDI_NAME)
   struct backing_dev_info bdi;
#endif
   bool haveRootEntryInfo; // false until the root EntryInfo is set in root-FhgfsInode

   bool isRootInited; /* false until root inode attrs have been fetched/initialized in
      _lookup() (because kernel does not automatically lookup/revalidate the root inode) */

};

/* Note: Functions below rely on sb->s_fs_info. Umount operations / super-block destroy operations
 *       in FhgfsOpsSuper.c might be more obvious if we would test here if sb->fs_info is NULL.
 *       However, the functions below are frequently called from various parts of the file system
 *       and additional checks might cause an overhead. Therefore those checks are not done
 *       here.
 */

/**
 * NOTE: Make sure sb->s_fs_info is initialized!
 */
App* FhgfsOps_getApp(struct super_block* sb)
{
   FhgfsSuperBlockInfo* sbInfo = sb->s_fs_info;

   return &(sbInfo->app);
}

/**
 * NOTE: Make sure sb->s_fs_info is initialized!
 */
struct backing_dev_info* FhgfsOps_getBdi(struct super_block* sb)
{
#if defined(KERNEL_HAS_SUPER_SETUP_BDI_NAME)
   return sb->s_bdi;
#else
   FhgfsSuperBlockInfo* sbInfo = sb->s_fs_info;

   return &(sbInfo->bdi);
#endif
}

/**
 * Note: Do not get it (reliably) without also locking fhgfsInode->entryInfoLock!
 */
bool FhgfsOps_getHasRootEntryInfo(struct super_block* sb)
{
   FhgfsSuperBlockInfo* sbInfo = sb->s_fs_info;
   return sbInfo->haveRootEntryInfo;
}

/**
 * Note: Do not set it without also locking fhgfsInode->entryInfoLock! Exception is during the
 *       mount process.
 */
void FhgfsOps_setHasRootEntryInfo(struct super_block* sb, bool haveRootEntryInfo)
{
   FhgfsSuperBlockInfo* sbInfo = sb->s_fs_info;
   sbInfo->haveRootEntryInfo = haveRootEntryInfo;
}


bool FhgfsOps_getIsRootInited(struct super_block* sb)
{
   FhgfsSuperBlockInfo* sbInfo = sb->s_fs_info;
   return sbInfo->isRootInited;
}

void FhgfsOps_setIsRootInited(struct super_block* sb, bool isInited)
{
   FhgfsSuperBlockInfo* sbInfo = sb->s_fs_info;
   sbInfo->isRootInited = isInited;
}

#endif /* FHGFSOPSSUPER_H_ */
